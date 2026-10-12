#!/usr/bin/env python3
"""notarise.py — NOTARISER UN FICHIER PAR L'API WEB D'APPLE, sans notarytool.

POURQUOI CE SCRIPT. La notarisation se fait d'ordinaire avec `xcrun notarytool`
(docs/livraison.md, étape 4). Mais notarytool n'existe que depuis Xcode 13, qui
demande macOS 11.3 ; et l'ancien outil, `altool`, n'est plus accepté par Apple
depuis le 1er novembre 2023. Sur un Mac resté sous Catalina avec Xcode 12.4 —
celui de l'utilisatrice, le 12 octobre —, il ne reste que l'API web d'Apple,
la « Notary API », que notarytool appelle lui-même. Ce script l'appelle aussi.

RIEN À INSTALLER : Python 3.8 (celui d'Xcode 12.4) et sa bibliothèque standard,
plus `openssl` et `curl`, livrés avec macOS. `openssl` signe le jeton d'accès ;
`curl` parle à Apple et à Amazon, avec les certificats du système — le Python
d'Xcode ne les trouve pas toujours.

CE QU'IL FAIT, comme notarytool :
  1. un jeton d'accès (JWT, ES256) signé avec la clé d'API App Store Connect ;
  2. une demande de soumission, avec l'empreinte SHA-256 du fichier : Apple
     répond par des identifiants temporaires de dépôt chez Amazon S3 ;
  3. l'envoi du fichier sur S3, signé à la main (AWS Signature Version 4) ;
  4. l'attente du verdict, puis le journal d'Apple.
Il reste ensuite à AGRAFER le ticket : `xcrun stapler staple <fichier>`, qui
existe, lui, dans Xcode 12.4.

CE QUI EST VÉRIFIÉ, ET CE QUI NE L'EST PAS. La signature S3 a été comparée,
octet pour octet, à celle de botocore, la bibliothèque officielle d'Amazon ; le
jeton, vérifié avec une clé de test. Mais le script n'a JAMAIS PARLÉ À APPLE au
moment où il est écrit : faute d'identifiants, personne ne l'a lancé pour de
vrai. `--essai` fait tout sauf le réseau.

UTILISATION :
  python3 outils/notarise.py HC-0.8.3.dmg \\
      --cle ~/cles/AuthKey_ABC123DEFG.p8 --id-cle ABC123DEFG \\
      --emetteur 69a6de70-0000-0000-0000-000000000000

La clé (.p8), son identifiant et l'identifiant de l'émetteur viennent d'App
Store Connect → Utilisateurs et accès → Intégrations → API App Store Connect.
"""
import argparse
import base64
import datetime
import hashlib
import hmac
import json
import os
import re
import subprocess
import sys
import tempfile
import time
import urllib.parse

API = "https://appstoreconnect.apple.com/notary/v2"
REGION_S3 = "us-west-2"          # celle du dépôt d'Apple ; corrigée si S3 en dit une autre
ATTENTE_MAX = 3600               # une heure, en secondes : la notarisation prend des minutes


def b64url(octets):
    return base64.urlsafe_b64encode(octets).rstrip(b"=").decode("ascii")


# ------------------------------------------------------------------ le jeton

def der_vers_brut(der):
    """Une signature ECDSA, du DER d'openssl — SEQUENCE { INTEGER r, INTEGER s }
    — vers le format des JWT : r puis s, 32 octets chacun."""
    if len(der) < 8 or der[0] != 0x30:
        raise ValueError("signature DER illisible")
    i = 2 if der[1] < 0x80 else 2 + (der[1] & 0x7F)
    nombres = []
    for _ in range(2):
        if der[i] != 0x02:
            raise ValueError("signature DER illisible")
        lon = der[i + 1]
        n = der[i + 2:i + 2 + lon].lstrip(b"\x00")
        if len(n) > 32:
            raise ValueError("signature DER illisible")
        nombres.append(n.rjust(32, b"\x00"))
        i += 2 + lon
    return nombres[0] + nombres[1]


def jeton(cle, id_cle, emetteur, maintenant=None):
    """Le jeton d'accès à l'API, valable quinze minutes (Apple en accepte vingt)."""
    t = int(time.time()) if maintenant is None else maintenant
    entete = {"alg": "ES256", "kid": id_cle, "typ": "JWT"}
    charge = {"iss": emetteur, "iat": t, "exp": t + 15 * 60, "aud": "appstoreconnect-v1"}
    corps = (b64url(json.dumps(entete, separators=(",", ":")).encode()) + "." +
             b64url(json.dumps(charge, separators=(",", ":")).encode()))
    p = subprocess.run(["openssl", "dgst", "-sha256", "-sign", cle],
                       input=corps.encode(), stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE)
    if p.returncode != 0:
        sys.exit("openssl n'a pas pu signer avec %s :\n%s" % (cle, p.stderr.decode(errors="replace")))
    return corps + "." + b64url(der_vers_brut(p.stdout))


# ------------------------------------------------------------------ le réseau

def curl(arguments, entree=None):
    """Un appel à curl. Rend (code HTTP, corps de la réponse)."""
    with tempfile.NamedTemporaryFile(delete=False) as sortie:
        chemin = sortie.name
    try:
        p = subprocess.run(["curl", "-sS", "--max-time", "600", "-o", chemin,
                            "-w", "%{http_code}"] + arguments,
                           input=entree, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if p.returncode != 0:
            sys.exit("curl a échoué : %s" % p.stderr.decode(errors="replace").strip())
        with open(chemin, "rb") as f:
            corps = f.read()
        return int(p.stdout.decode().strip() or 0), corps
    finally:
        os.unlink(chemin)


def apple(methode, chemin, cles, corps=None):
    """Une requête à l'API de notarisation, avec un jeton neuf à chaque fois."""
    j = jeton(*cles)
    args = ["-X", methode, API + chemin, "-H", "Authorization: Bearer " + j]
    entree = None
    if corps is not None:
        args += ["-H", "Content-Type: application/json", "--data-binary", "@-"]
        entree = json.dumps(corps).encode()
    code, reponse = curl(args, entree)
    if code // 100 != 2:
        sys.exit("Apple répond %d à %s %s :\n%s" % (code, methode, chemin,
                                                    reponse.decode(errors="replace")))
    return json.loads(reponse or b"{}")


# ------------------------------------------------------------------ S3, signé à la main

def _hmac(cle, texte):
    return hmac.new(cle, texte.encode("utf-8"), hashlib.sha256).digest()


def signature_s3(methode, hote, chemin, region, ident, secret, jeton_session,
                 empreinte, quand):
    """Les en-têtes d'une requête S3 signée — AWS Signature Version 4.

    `chemin` est la clé de l'objet, déjà encodée pour l'URL ; `empreinte`, le
    SHA-256 du contenu, en hexadécimal ; `quand`, un datetime UTC. Rend le
    dictionnaire des en-têtes à envoyer, Authorization compris."""
    date_longue = quand.strftime("%Y%m%dT%H%M%SZ")
    date_courte = quand.strftime("%Y%m%d")
    entetes = {
        "host": hote,
        "x-amz-content-sha256": empreinte,
        "x-amz-date": date_longue,
        "x-amz-security-token": jeton_session,
    }
    noms = sorted(entetes)
    canonique = "\n".join([
        methode,
        chemin,
        "",                                            # pas de paramètres
        "".join("%s:%s\n" % (n, entetes[n].strip()) for n in noms),
        ";".join(noms),
        empreinte,
    ])
    portee = "%s/%s/s3/aws4_request" % (date_courte, region)
    a_signer = "\n".join(["AWS4-HMAC-SHA256", date_longue, portee,
                          hashlib.sha256(canonique.encode("utf-8")).hexdigest()])
    k = _hmac(("AWS4" + secret).encode("utf-8"), date_courte)
    k = _hmac(k, region)
    k = _hmac(k, "s3")
    k = _hmac(k, "aws4_request")
    sig = hmac.new(k, a_signer.encode("utf-8"), hashlib.sha256).hexdigest()
    entetes["Authorization"] = ("AWS4-HMAC-SHA256 Credential=%s/%s, SignedHeaders=%s, "
                                "Signature=%s" % (ident, portee, ";".join(noms), sig))
    return entetes


def depose_s3(fichier, empreinte, attributs, region=REGION_S3, deja=False):
    """Envoie le fichier dans le dépôt qu'Apple vient d'ouvrir."""
    seau, objet = attributs["bucket"], attributs["object"]
    hote = "%s.s3.%s.amazonaws.com" % (seau, region)
    chemin = "/" + urllib.parse.quote(objet, safe="/-_.~")
    entetes = signature_s3("PUT", hote, chemin, region,
                           attributs["awsAccessKeyId"], attributs["awsSecretAccessKey"],
                           attributs["awsSessionToken"], empreinte,
                           datetime.datetime.now(datetime.timezone.utc))
    args = ["-X", "PUT", "--upload-file", fichier, "https://" + hote + chemin]
    for n, v in entetes.items():
        if n != "host":
            args += ["-H", "%s: %s" % (n, v)]
    code, reponse = curl(args)
    if code // 100 == 2:
        return
    # S3 dit sa région quand on s'est trompé : une seconde chance, pas plus.
    m = re.search(rb"<Region>([a-z0-9-]+)</Region>", reponse)
    if m and not deja and m.group(1).decode() != region:
        print("   S3 demande la région %s : nouvel essai" % m.group(1).decode())
        return depose_s3(fichier, empreinte, attributs, m.group(1).decode(), True)
    sys.exit("Amazon S3 répond %d :\n%s" % (code, reponse.decode(errors="replace")))


# ------------------------------------------------------------------ le tout

def empreinte_de(fichier):
    h = hashlib.sha256()
    with open(fichier, "rb") as f:
        for bloc in iter(lambda: f.read(1 << 20), b""):
            h.update(bloc)
    return h.hexdigest()


def main():
    p = argparse.ArgumentParser(description="Notariser un fichier par l'API web d'Apple.")
    p.add_argument("fichier", help="le DMG signé, ou un ZIP de l'app signée")
    p.add_argument("--cle", required=True, help="la clé d'API App Store Connect (AuthKey_….p8)")
    p.add_argument("--id-cle", required=True, help="l'identifiant de la clé (Key ID)")
    p.add_argument("--emetteur", required=True, help="l'identifiant de l'émetteur (Issuer ID)")
    p.add_argument("--essai", action="store_true",
                   help="tout préparer et ne rien envoyer : vérifie la clé et le fichier")
    a = p.parse_args()

    if not os.path.isfile(a.fichier):
        sys.exit("fichier introuvable : %s" % a.fichier)
    cles = (os.path.expanduser(a.cle), a.id_cle, a.emetteur)
    nom = os.path.basename(a.fichier)
    empreinte = empreinte_de(a.fichier)
    print("fichier   %s (%d octets)\nSHA-256   %s" % (nom, os.path.getsize(a.fichier), empreinte))

    j = jeton(*cles)
    print("jeton     signé (%d caractères)" % len(j))
    if a.essai:
        entete, charge = (json.loads(base64.urlsafe_b64decode(x + "=" * (-len(x) % 4)))
                          for x in j.split(".")[:2])
        print("en-tête   %s\ncharge    %s" % (json.dumps(entete), json.dumps(charge)))
        print("\n--essai : rien n'a été envoyé.")
        return 0

    print("\n1. demande de soumission à Apple")
    r = apple("POST", "/submissions", cles, {"submissionName": nom, "sha256": empreinte})
    ident = r["data"]["id"]
    print("   soumission %s" % ident)

    print("2. envoi du fichier")
    depose_s3(a.fichier, empreinte, r["data"]["attributes"])
    print("   envoyé")

    print("3. attente du verdict (quelques minutes d'ordinaire)")
    debut = time.time()
    statut = "In Progress"
    while statut == "In Progress":
        if time.time() - debut > ATTENTE_MAX:
            sys.exit("toujours en cours après une heure. Soumission : %s" % ident)
        time.sleep(20)
        statut = apple("GET", "/submissions/" + ident, cles)["data"]["attributes"]["status"]
        print("   %s (%d s)" % (statut, time.time() - debut))

    print("4. le journal d'Apple")
    url = apple("GET", "/submissions/%s/logs" % ident, cles)["data"]["attributes"].get("developerLogUrl")
    if url:
        code, journal = curl([url])
        try:
            print(json.dumps(json.loads(journal), indent=2, ensure_ascii=False))
        except ValueError:
            print(journal.decode(errors="replace"))

    if statut == "Accepted":
        print("\nACCEPTÉ. Il reste à agrafer le ticket :\n   xcrun stapler staple %s" % a.fichier)
        return 0
    print("\nREFUSÉ (%s). Le journal ci-dessus dit pourquoi." % statut)
    return 1


if __name__ == "__main__":
    sys.exit(main())

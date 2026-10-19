#!/bin/sh
# Make the Rudolf OS Secure Boot key (Rudolf OS 1.2.2). Run it on your own computer, in a folder OUTSIDE the
# repository, with OpenSSL (Linux, macOS, or Git Bash on Windows):
#     sh mksbkey.sh
# It writes:
#   rudolf-sb.key   the PRIVATE key (RSA-2048). Never commit it, never share it. Keep it offline with a backup:
#                   if it is lost, every computer has to enroll a new key; if it leaks, someone can make
#                   software those computers start with Secure Boot on.
#   rudolf-sb.crt   the certificate (public). It goes into the repository as iso/boot/sb/rudolf-sb.crt.
#   rudolf.cer      the same certificate in the form MokManager reads (made from rudolf-sb.crt by the build too).
# The private key goes into the repository secret RUDOLF_SB_KEY (the whole file, BEGIN and END lines included).
# GPL-2.0-or-later.
set -e
command -v openssl >/dev/null || { echo "OpenSSL is needed (on Windows: use Git Bash, which includes it)."; exit 1; }
for f in rudolf-sb.key rudolf-sb.crt rudolf.cer; do
	[ -e "$f" ] && { echo "$f already exists here; move it away first (this would replace your key)."; exit 1; }
done
umask 077
cat > rudolf-sb.cnf <<'EOF'
[req]
distinguished_name = dn
prompt = no
x509_extensions = ext
[dn]
CN = Rudolf OS Secure Boot 2026
O = Rudolf OS
[ext]
basicConstraints = critical, CA:FALSE
keyUsage = critical, digitalSignature
extendedKeyUsage = codeSigning
subjectKeyIdentifier = hash
EOF
# shim and MokManager accept RSA-2048 with SHA-256; firmware does not check the dates, so the certificate is
# made to last 30 years (the key can still be replaced by enrolling a new one).
openssl req -new -x509 -newkey rsa:2048 -nodes -sha256 -days 10950 -config rudolf-sb.cnf \
	-keyout rudolf-sb.key -out rudolf-sb.crt
openssl x509 -in rudolf-sb.crt -outform DER -out rudolf.cer
rm -f rudolf-sb.cnf
chmod 600 rudolf-sb.key; chmod 644 rudolf-sb.crt rudolf.cer
echo
echo "Done. Certificate fingerprint (SHA-256):"
openssl x509 -in rudolf-sb.crt -noout -fingerprint -sha256 | sed 's/.*=//'
echo
echo "Next: 1) put the whole of rudolf-sb.key into the repository secret RUDOLF_SB_KEY;"
echo "      2) add rudolf-sb.crt to the repository as iso/boot/sb/rudolf-sb.crt (or paste it to Claude);"
echo "      3) move rudolf-sb.key somewhere safe and offline, with a backup."

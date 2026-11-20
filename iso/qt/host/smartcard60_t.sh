#!/bin/sh
# 1.6.0 (plan-1.6.0 Part 20): /usr/lib/rudolf/smartcard with NSS's own certutil and modutil, as the desktop's user:
# the card's module registered once in a new certificate database; the DoD bundle found on the library page, unzipped,
# its DER PKCS#7 picked; a stranger's address refused; certificates installed (a root "C,,", a CA ",,"), listed and
# removed; status as JSON with no pcscd.   sh smartcard60_t.sh   (python3 with cryptography, certutil, modutil, unzip)
HERE=$(cd "$(dirname "$0")" && pwd); R=$(cd "$HERE/../../.." && pwd); SC=$R/iso/sys/root/usr/lib/rudolf/smartcard
T=$(mktemp -d); P=0; F=0; trap 'rm -rf "$T"' EXIT
ok() { if [ "$2" = 1 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 ($3)"; fi; }
MOD=$(ls /usr/lib/*/pkcs11/p11-kit-trust.so 2>/dev/null | head -1)   # (a PKCS#11 module that loads anywhere, in OpenSC's place)
export RUDOLF_SC_USER=root RUDOLF_SC_HOME=$T/home RUDOLF_PKCS11=$MOD; mkdir -p $T/home
python3 "$HERE/mkscpki.py" $T/pki >/dev/null || { echo "FAIL the test PKI"; exit 1; }
sh $SC nss; sh $SC nss; n=$(modutil -dbdir sql:$T/home/.pki/nssdb -list 2>/dev/null | grep -c 'Smart cards')
ok "1. the card's module in a new certificate database, once however often it is asked" "$( [ "$n" = 1 ] && echo 1)" "$n"
# the bundle: a library page with the link, a zip with a PEM and a DER PKCS#7 (the DER is the one taken)
mkdir $T/z; cp $T/pki/bundle.p7b "$T/z/Certificates_PKCS7_v5_14_DoD.der.p7b"; printf 'x' > "$T/z/Certificates_PKCS7_v5_14_DoD.pem.p7b"
(cd $T/z && zip -q ../dod.zip *.p7b)
printf '<a href="https://dl.dod.cyber.mil/wp-content/uploads/pki-pke/zip/unclass-certificates_pkcs7_v5-14_dod.zip">PKCS#7</a>' > $T/lib.html
cat > $T/get <<Y
#!/bin/sh
case "\$1" in *pkipke-document-library*) cp $T/lib.html "\$2";; *v5-14_dod.zip) cp $T/dod.zip "\$2";; *) exit 1;; esac
Y
chmod +x $T/get
out=$(RUDOLF_TEST_FETCH=$T/get sh $SC dod-fetch $T/got.p7b)
ok "2. the bundle's address read from the DoD's library page, its DER PKCS#7 taken" "$(cmp -s $T/got.p7b $T/pki/bundle.p7b && [ "$out" = https://dl.dod.cyber.mil/wp-content/uploads/pki-pke/zip/unclass-certificates_pkcs7_v5-14_dod.zip ] && echo 1)" "$out"
printf '<a href="https://evil.example/certificates_pkcs7_dod.zip">x</a>' > $T/lib.html
out=$(RUDOLF_TEST_FETCH=$T/get RUDOLF_DOD_ZIP=https://evil.example/certificates_pkcs7_dod.zip sh $SC dod-fetch $T/got2.p7b); rc=$?
ok "3. an address that is not the DoD's refused" "$( [ $rc != 0 ] && [ ! -e $T/got2.p7b ] && echo "$out" | grep -q "not the DoD's" && echo 1)" "rc $rc $out"
mkdir $T/in; cp $T/pki/root.der $T/in/root-0.der; cp $T/pki/inter.der $T/in/ca-1.der
n=$(sh $SC dod-install $T/in)
L=$(certutil -d sql:$T/home/.pki/nssdb -L)
ok "4. installed: the root trusted for websites (C,,), the CA without trust of its own" "$( [ "$n" = 2 ] && echo "$L" | grep -E '^Rudolf DoD root-0 +C,, *$' >/dev/null && echo "$L" | grep -E '^Rudolf DoD ca-1 +,, *$' >/dev/null && echo 1)" "$n $L"
ok "5. listed" "$( [ "$(sh $SC dod-list | tr '\n' ' ')" = "Rudolf DoD root-0 Rudolf DoD ca-1 " ] && echo 1)" "$(sh $SC dod-list | tr '\n' ' ')"
sh $SC dod-remove
ok "6. removed, and nothing else touched (the module stays)" "$( [ -z "$(sh $SC dod-list)" ] && modutil -dbdir sql:$T/home/.pki/nssdb -list 2>/dev/null | grep -q 'Smart cards' && echo 1)" "$(sh $SC dod-list)"
st=$(PATH=/nonexistent:$PATH sh $SC status)
ok "7. status without pcscd: JSON, no readers" "$(echo "$st" | python3 -c 'import json,sys;d=json.load(sys.stdin);print(1 if d["ok"] and d["readers"]==[] else 0)')" "$st"
echo "$P passed, $F failed"; [ $F = 0 ]

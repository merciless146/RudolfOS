# 1.6.0 (docs/plan-1.6.0.md Part 14): an app using the keyring through the Secret Service (python3-secretstorage, as
# libsecret would): one step per run, its outcome printed.   python3 krapp.py create LABEL | store LABEL | lock LABEL |
# unlock LABEL | read LABEL
import sys, secretstorage
from secretstorage.exceptions import PromptDismissedException
op, label = sys.argv[1], sys.argv[2]
c = secretstorage.dbus_init()
def find():
    for col in secretstorage.get_all_collections(c):
        if col.get_label() == label: return col
try:
    if op == "create":
        col = secretstorage.create_collection(c, label, alias="")
        print("created" if col.get_label() == label else "wrong label")
    elif op == "store":
        find().create_item("Work sign-in", {"application": "krapp", "account": "ann"}, b"work-token-7"); print("stored")
    elif op == "lock":
        find().lock(); print("locked" if find().is_locked() else "not locked")
    elif op == "unlock":
        dismissed = find().unlock(); print("dismissed" if dismissed else "unlocked" if not find().is_locked() else "still locked")
    elif op == "read":
        col = find(); print("locked" if col.is_locked() else [i.get_secret().decode() for i in col.get_all_items()])
except PromptDismissedException: print("dismissed")
except Exception as e: print("error " + type(e).__name__ + ": " + str(e)[:300])

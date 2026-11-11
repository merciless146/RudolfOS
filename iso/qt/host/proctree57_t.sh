#!/bin/sh
# 1.5.7: Task Manager's End process tree (rudolfd proc/end?tree=1): the process and every process it started, and
# theirs, ended; processes Rudolf OS needs left running with what runs under them; nothing outside the tree touched.
# rudolfd's own functions (proc_tree, proc_tree_end) on a real process tree.   sh proctree57_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); C=$HERE/../../sys/rudolfd.c
W=$(mktemp -d); P=0; F=0
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
{ printf '#include <%s>\n' stdio.h stdlib.h string.h signal.h dirent.h unistd.h
  grep '^struct pinfo {' "$C"
  sed -n '/^static int proc_tree(/,/^}/p;/^static int proc_tree_end(/,/^}/p' "$C"
  cat <<'X'
/* the processes now (pid, ppid); PROT: a pid marked as one Rudolf OS needs */
int main(int c, char **v) {
	static struct pinfo ps[4096]; int n = 0, root = atoi(v[1]), prot = c > 2 ? atoi(v[2]) : 0; DIR *d = opendir("/proc"); struct dirent *e;
	while ((e = readdir(d)) && n < 4096) { int pid = atoi(e->d_name); if (pid <= 0) continue; char p[64], b[512]; snprintf(p, sizeof p, "/proc/%d/stat", pid);
		FILE *f = fopen(p, "r"); if (!f) continue; if (!fgets(b, sizeof b, f)) { fclose(f); continue; } fclose(f);
		char *q = strrchr(b, ')'); if (!q) continue; memset(&ps[n], 0, sizeof ps[n]); ps[n].pid = pid; sscanf(q + 2, "%*c %d", &ps[n].ppid); ps[n].prot = pid == prot; n++; }
	closedir(d);
	int order[4096], kept = 0, no = proc_tree(ps, n, root, order, &kept);
	printf("tree"); for (int k = 0; k < no; k++) printf(" %d", ps[order[k]].pid); printf("\nkept %d\nended %d\n", kept, proc_tree_end(ps, order, no, 0)); return 0; }
X
} > $W/t.c
gcc -w -o $W/t $W/t.c || { echo "FAIL not built"; exit 1; }
# a tree: A starts B and C; B starts D; and E, outside the tree
sh -c 'sh -c "sleep 300 & echo \$! > '$W'/d; wait" & echo $! > '$W'/b; sleep 300 & echo $! > '$W'/c; wait' & A=$!
sleep 300 & E=$!; sleep 0.5; B=$(cat $W/b); PC=$(cat $W/c); D=$(cat $W/d)
o=$($W/t $A); sleep 0.5
gone() { ! kill -0 $1 2>/dev/null || [ "$(awk '{print $3}' /proc/$1/stat 2>/dev/null)" = Z ]; }
ok "1. the process and all it started, and theirs, are ended" $(gone $A && gone $B && gone $PC && gone $D; echo $?) "$o"
ok "2. each after its parent in the list (children ended first)" $(echo "$o" | grep -q "^tree $A .*$B" && echo "$o" | awk -v d=$D -v b=$B '/^tree/{for(i=2;i<=NF;i++){if($i==b)bi=i;if($i==d)di=i}} END{exit !(di>bi)}'; echo $?) "$o"
ok "3. a process outside the tree is not touched" $(kill -0 $E 2>/dev/null; echo $?)
# a process Rudolf OS needs inside the tree: left running, with what runs under it
sh -c 'sh -c "sleep 300 & echo \$! > '$W'/d2; wait" & echo $! > '$W'/b2; wait' & A2=$!; sleep 0.5; B2=$(cat $W/b2); D2=$(cat $W/d2)
o=$($W/t $A2 $B2); sleep 0.5
ok "4. a process Rudolf OS needs is left running, with what runs under it, and counted" $(gone $A2 && kill -0 $B2 && kill -0 $D2 && echo "$o" | grep -q '^kept 1'; echo $?) "$o"
kill $E $B2 $D2 2>/dev/null; wait 2>/dev/null
grep -qF 'qparam(r->query, "tree")' "$C" && grep -qF 'endProc(cur(), false, true)' "$HERE/../../../desktop/tm26.js"; ok "5. rudolfd's proc/end takes tree=1, and Task Manager has End process tree" $?
rm -rf "$W"; echo "$P/$((P+F)) passed"; [ $F = 0 ]

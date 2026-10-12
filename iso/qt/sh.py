import sys;sys.path.insert(0,'.')
from ser import Ser
s=Ser(sys.argv[1]);s.read(0.5)
print(s.run(sys.argv[2],float(sys.argv[3]) if len(sys.argv)>3 else 5))

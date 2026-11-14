# Rudolf, majestic: the white low-poly stag (facing left) with his head high, chest out, a front hoof up on a ledge and
# a full rack of antlers. 200x200 box; hooves rest at y=152 (ground) and y=140 (the ledge, front near leg).
HOOVES=[(64,70,140),(87,95,152),(117,124,152),(128,135,152)]   # (x left, x right, y) of each hoof's sole
def stag(fill='#f3f6ff',shade='#c9d3ee',far='#b7c2e2',ant='#eef2ff'):
    far_legs=("M86,116 L91,134 L87,152 L95,152 L97,134 L95,116 Z "
              "M126,118 L131,134 L128,152 L135,152 L136,134 L134,116 Z")
    body=("M38,51 L40,57 L50,60 L59,67 L65,82 L68,98 L73,111 L76,117 L66,124 L65,138 L64,140 L70,140 L72,127 L83,119 "
          "L92,121 L110,121 L116,121 L118,136 L117,152 L124,152 L124,138 L128,130 L134,121 L138,106 L136,94 L140,91 L137,87 "
          "L121,84 L92,81 L79,70 L66,53 L70,45 L62,49 L58,50 L53,50 Z")
    facets=["M65,82 L92,81 L104,84 L86,104 L68,98 Z","M86,104 L104,84 L118,92 L110,121 L92,121 L83,119 Z",
            "M121,84 L136,94 L138,106 L126,104 L118,92 Z","M38,51 L53,50 L50,60 L40,57 Z","M59,67 L79,70 L65,82 Z",
            "M68,98 L86,104 L83,119 L76,117 L73,111 Z"]
    eye="M49,53 L53,52 L52,54.5 Z"
    antlers=[(59,50,[(64,37),(71,25),(75,12)]),(63,39,[(54,29)]),(67,31,[(62,17)]),(71,25,[(84,18)]),(65,36,[(77,34)]),
             (55,51,[(49,38),(43,26),(35,16)]),(50,41,[(40,38)]),(46,32,[(48,20)]),(43,26,[(31,28)])]
    s=f"<path d='{far_legs}' fill='{far}'/><path d='{body}' fill='{fill}'/>"
    s+=''.join(f"<path d='{f}' fill='{shade}' opacity='.55'/>" for f in facets)
    s+=f"<path d='{eye}' fill='#1b2440'/>"
    for x,y,pts in antlers:
        d=f"M{x},{y} "+' '.join(f"L{a},{b}" for a,b in pts)
        s+=f"<path d='{d}' fill='none' stroke='{ant}' stroke-width='2.2' stroke-linecap='round' stroke-linejoin='round'/>"
    return s
NOSE=(39.2,53.6)

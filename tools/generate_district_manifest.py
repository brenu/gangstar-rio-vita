#!/usr/bin/env python3
"""Generate metadata-only district asset locations from the supported original OBB."""
import argparse
from pathlib import Path
from inventory_textures import entries,WORLD
p=argparse.ArgumentParser();p.add_argument('obb',type=Path);p.add_argument('--output',type=Path,default=Path('source/gangstar_district_manifest.h'));args=p.parse_args()
if args.obb.stat().st_size!=2119380688:raise SystemExit('Unsupported original OBB size')
rows=[x for x in entries(args.obb) if WORLD.match(Path(x['name']).name.lower()) and x['bytes'] in (2796344,699192,349672) and x['name'].lower().endswith('.tga') and not x['flags']&0x10000]
if len(rows)!=322:raise SystemExit('Unsupported district directory')
s=['// Generated from immutable supported OBB directory; names/locations only.','#ifndef GANGSTAR_DISTRICT_MANIFEST_H','#define GANGSTAR_DISTRICT_MANIFEST_H','static const struct {const char *name;uint32_t offset,size;} rio_district_assets[]={']
s+=[' {"%s",%du,%du},'%(Path(x['name']).name.lower(),x['offset'],x['bytes']) for x in rows]
s+=['};','#endif'];args.output.write_text('\n'.join(s)+'\n')

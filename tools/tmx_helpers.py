"""Game-owned TMX build helpers. Tile bytes are produced by the SDK's tiled2zeal.

Maps use one 16px CSV layer named Tiles, one tileset (firstgid=1), and point
objects. CMake's SDK tiled2zeal() helper owns conversion; these helpers read its
outputs, normalize erased cells to blank tile 0, and pack game metadata.
"""
from pathlib import Path
import argparse
import json
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


def options():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--tiles', type=Path, required=True, help='directory of SDK-exported ZTM files')
    a = p.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    return a


def write(path, data):
    if isinstance(data, str): data = data.encode('utf-8')
    if not path.exists() or path.read_bytes() != data: path.write_bytes(data)


def properties(node):
    result = {}
    for p in node.findall('./properties/property'):
        value = p.get('value', p.text or '')
        if p.get('type') == 'int': value = int(value)
        if p.get('type') == 'bool': value = value == 'true'
        result[p.get('name')] = value
    return result


class Map:
    def __init__(self, path, dimensions=None):
        self.path = path
        self.root = ET.parse(path).getroot()
        r = self.root
        self.w, self.h = int(r.get('width')), int(r.get('height'))
        if r.get('orientation') != 'orthogonal' or r.get('infinite', '0') != '0' or r.get('tilewidth') != '16' or r.get('tileheight') != '16':
            raise ValueError(f'{path}: expected a finite orthogonal map with 16x16 tiles')
        if dimensions and (self.w, self.h) != dimensions:
            raise ValueError(f'{path}: expected {dimensions}, got {(self.w,self.h)}')
        layers = r.findall('layer')
        if r.findall('group') or len(layers) != 1 or layers[0].get('name') != 'Tiles':
            raise ValueError(f'{path}: expected one top-level Tiles layer; object layers are separate')
        self.layer = layers[0]
        if any(float(self.layer.get(k, '0')) for k in ['x','y','offsetx','offsety']):
            raise ValueError(f'{path}: tile-layer offsets are unsupported')
        if (int(self.layer.get('width', '0')), int(self.layer.get('height', '0'))) != (self.w, self.h):
            raise ValueError(f'{path}: tile layer dimensions must match the map')
        data = self.layer.find('data')
        if data is None or data.get('encoding') != 'csv':
            raise ValueError(f'{path}: save tile layer data as CSV')
        self.gids = [int(s) for s in data.text.replace('\n','').split(',') if s.strip()]
        sets = r.findall('tileset')
        if len(sets) != 1 or sets[0].get('firstgid') != '1' or not sets[0].get('source'):
            raise ValueError(f'{path}: expected one external tileset with firstgid=1')
        tileset = ET.parse(path.parent/sets[0].get('source')).getroot()
        count = int(tileset.get('tilecount'))
        if len(self.gids) != self.w*self.h or any(g < 0 or g > min(count,256) for g in self.gids):
            raise ValueError(f'{path}: invalid tile count/ID, flipped tile, or index above 255')
        self.props = properties(r)
        self.objects = []
        for group in r.findall('objectgroup'):
            if any(float(group.get(k,'0')) for k in ['x','y','offsetx','offsety']):
                raise ValueError(f'{path}: object-layer offsets are unsupported')
            for obj in group.findall('object'):
                x, y = float(obj.get('x','0'))/16, float(obj.get('y','0'))/16
                if obj.find('point') is None or obj.get('gid') or float(obj.get('rotation','0')) or not x.is_integer() or not (y*4).is_integer() or not 0 <= x < self.w or not 0 <= y < self.h:
                    raise ValueError(f'{path}: object {obj.get("id")} must be an unrotated, grid-aligned point inside the map')
                self.objects.append(dict(kind=obj.get('class',obj.get('type','')), x=int(x), y=int(y) if y.is_integer() else y, props=properties(obj), layer=group.get('name')))

    def tiles(self, args, screen=None):
        path = args.tiles / (self.path.stem + '.ztm')
        result = bytearray(path.read_bytes())
        if len(result) != self.w * self.h:
            raise ValueError(f'{path}: expected {self.w * self.h} native tile bytes')
        # The native converter emits 255 for erased cells. These games use tile 0
        # for blank; inspect the source GID so actual tile ID 255 stays intact.
        for i, gid in enumerate(self.gids):
            if gid == 0:
                result[i] = 0
        if screen:
            sw, sh = screen
            if self.w % sw or self.h % sh:
                raise ValueError(f'{self.path}: incomplete screen page')
            result = b''.join(result[y * self.w + x:y * self.w + x + sw]
                              for sy in range(0, self.h, sh)
                              for x in range(0, self.w, sw)
                              for y in range(sy, sy + sh))
        return bytes(result)

    def named_objects(self, names, layer='Spawns'):
        result = [o for o in self.objects if o['layer']==layer]
        if any(not isinstance(o['y'], int) and o['kind'] != 'Zip' for o in result):
            raise ValueError(f'{self.path}: only Zip objects support quarter-tile y positions')
        unknown = [o['kind'] for o in result if o['kind'] not in names]
        if unknown: raise ValueError(f'{self.path}: unknown object classes: {unknown}')
        return sorted(result, key=lambda o:(o['y'],o['x']))


def objects_file(args, maps, names, stem, courier=False, values=None):
    values = list(range(len(names))) if values is None else values
    records = [m.named_objects(names) for m in maps]
    for m,items in zip(maps,records):
        count = sum(o['kind']=='Player' for o in items)
        if (not courier and count!=1) or (courier and count>1): raise ValueError(f'{m.path}: invalid Player count')
    if courier and sum(o['kind']=='Player' for items in records for o in items)!=1: raise ValueError('world needs one Player')
    maximum = max(1,max(map(len,records)))
    if maximum>255: raise ValueError('too many objects in one map')
    data = bytearray()
    for m, objects in zip(maps, records):
        if courier:
            exits=[int(m.props.get(k,255)) for k in ['left','right','up','down']]
            if any(e!=255 and not 0<=e<len(maps) for e in exits): raise ValueError(f'{m.path}: bad exit')
            zone=int(m.props['zone'])
            if not 0<=zone<4:raise ValueError(f'{m.path}: invalid zone')
            data+=bytes(exits+[zone])
        data.append(len(objects))
        for o in objects:
            if not isinstance(o['y'],int): raise ValueError(f'{m.path}: spawn is not tile-aligned')
            data+=bytes([values[names.index(o['kind'])],o['x'],o['y']])
        data+=bytes((maximum-len(objects))*3)
    write(args.out/(stem+'.spn'),data)
    header='#ifndef MAP_DATA_H\n#define MAP_DATA_H\n#include <stdint.h>\n'
    header+='enum { '+', '.join('MAP_'+name.upper()+'='+str(value) for name,value in zip(names,values))+' };\n'
    header+='typedef struct { uint8_t kind, x, y; } map_object_t;\n'
    header+='typedef struct { '+('uint8_t exits[4], zone; ' if courier else '')+f'uint8_t count; map_object_t objects[{maximum}]; '+'} map_objects_t;\n#endif\n'
    write(args.out/'map_data.h',header)
    return records


def c_string(value):
    if not isinstance(value,str) or not value.isascii() or not value or len(value)>20:
        raise ValueError('level names must be 1..20 ASCII characters')
    return json.dumps(value)


def load_maps(pattern, count_macro=None, dimensions=None):
    maps = [Map(p,dimensions) for p in sorted((ROOT/'assets').glob(pattern))]
    if not maps: raise ValueError('no TMX maps found')
    if count_macro:
        import re
        count = int(re.search(r'^#define\s+'+count_macro+r'\s+(\d+)',(ROOT/'src/game.h').read_text(),re.M).group(1))
        if len(maps)!=count: raise ValueError(f'{count_macro} is {count}, found {len(maps)} maps')
    for i,m in enumerate(maps):
        if m.props.get('index')!=i: raise ValueError(f'{m.path}: expected index {i}')
    return maps


def constant(name):
    import re
    return int(re.search(r'^#define\s+'+name+r'\s+(\d+)',(ROOT/'src/game.h').read_text(),re.M).group(1))

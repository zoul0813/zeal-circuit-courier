#!/usr/bin/env python3
"""Pack SDK-exported tiles and metadata from git-tracked assets/rooms/*.tmx."""
from tmx_helpers import *
a=options(); maps=load_maps('rooms/room[0-9][0-9].tmx','ROOM_COUNT',(20,15))
write(a.out/'rooms.ztm',b''.join(m.tiles(a) for m in maps))
names=['Door','Socket','Switch','GateClosed','GateOpen','FanOn','FanOff','SocketFan','Drone','Bit','Player']
objects_file(a,maps,names,'rooms',courier=True,values=list(range(64,64+len(names))))

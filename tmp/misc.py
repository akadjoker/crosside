import io
import os
import sys
from subprocess import STDOUT, PIPE
import argparse
import shutil
import time
import json
import re
import subprocess
from subprocess import check_output, CalledProcessError, STDOUT
from datetime import datetime

import http.server
import socketserver


import zipfile


                
def fileNamesRetrieve( top, maxDepth, fnMask  ):
    for d in range( 1, maxDepth+1 ):
        maxGlob = "/".join( "*" * d )
        print(d)
        topGlob = os.path.join( top, maxGlob )
        allFiles = glob.glob( topGlob )
        print(allFiles)
        if fnmatch.fnmatch( os.path.basename( f ), fnMask ):
            yield f             



path = "/media/djoker/code/linux/python/projects/CppEditor/modules/"
''''
path = os.path.normpath(path)
res = []
for root,dirs,files in os.walk(path, topdown=True):
    depth = root[len(path) + len(os.path.sep):].count(os.path.sep)
    if depth == 0:
        for name in files:
            ext = os.path.splitext(name)[1]
            if (ext==".json"):
                print(name)
        continue
'''
def getFoldersTree(root,max_depth=0):
    path =root
    path = os.path.normpath(path)
    res = []
    for root,dirs,files in os.walk(path, topdown=True):
        depth = root[len(path) + len(os.path.sep):].count(os.path.sep)
        if depth == max_depth:
            res += [os.path.join(root, d) for d in dirs]
            dirs[:] = [] # Don't recurse any deeper
    
    return res
 
            
#dirs =getFoldersTree("/media/djoker/code/linux/python/projects/CppEditor/modules/",0)
#for name in dirs:
#    print(name)



'''
path = os.path.normpath("/media/djoker/code/linux/python/projects/CppEditor/modules/sdl2/src")
with open("src.txt","w") as data:
    for root, directories, files in os.walk(path, topdown=False):
        for name in files:
            ext = os.path.splitext(name)[1]
            if (ext==".c"):
                src=os.path.join(root, name)
            print(src)
            data.write(src+"\n")
'''
with open("/media/djoker/code/linux/python/projects/CppEditor/projects/raylib/Android/simple_game/AndroidManifest.xml") as data:
    
    match = re.compile('package="(.*?)".*?android:label="(.*?)".*?<activity android:name="(.*?)"',re.DOTALL).findall(str(data.readlines())) 
    print (match)
     
    

import os
import shutil
import subprocess
import json 
from getpass import getpass
import platform
import sys
import re


keystorePath = "./.secrets/keystore.json"
paths_to_include  = ["/code", "/data", "/docs", "/help", "/javascript", "/jsui", "/externals", "/media", "/misc", "/patchers", "/extras", "/icon.png", "/license.txt", "/README.md", "/package-info.json"]
externals = []
mac_externals = []



def macos_codesign():
    if (platform.system() != "Darwin"):
        print("not on mac os and connot sign")
        return
    macoskey = ""
    with open(keystorePath) as keystore:
        keys = json.load(keystore)
        macoskey = keys["developer_team_code"]

    if len(macoskey) <= 0:
        print("err: not valid key found")
        return
    
    dest = "./grainflow/grainflowExternals_temp"
    archive = "./grainflow/grainflowExternals"
    if os.path.isdir(dest): 
        shutil.rmtree(dest)
    os.mkdir(dest)

    externals = os.listdir("./externals")
    mac_externals =  [ s for s in externals if re.search(r"\.darwin-fat-32.so$", s) ]


    print(mac_externals)
    if (len(mac_externals) <= 0): return 
    for external in mac_externals:
        ex_path = f'./grainflow/{external}'
        cmd = f'sudo codesign -s {macoskey} --options runtime --timestamp --force --deep  -f {ex_path}'
        p = subprocess.Popen(cmd, shell=True)  
        p.wait()
        shutil.copytree(ex_path, dest + f'/{external}')
        

        
    print("zipping into archive for submission...")
    if os.path.isfile(archive+".zip"):  
        os.remove(archive+".zip")
    shutil.make_archive(archive, "zip", dest)

    print("Uploading for approval...")

    cmd = f'xcrun notarytool submit {archive}.zip --keychain-profile grainflow' 
    p = subprocess.Popen(cmd, shell=True)  
    p.wait()
    output, error = p.communicate()
    submissionid = input("ID: ")
    cmd = f'xcrun notarytool wait --keychain-profile grainflow {submissionid}'
    p = subprocess.Popen(cmd, shell=True)  
    p.wait()
    for external in mac_externals:
        shutil.rmtree(f'./externals/{external}')
    shutil.unpack_archive(archive+".zip", "./externals")
    mac_staple()
    shutil.rmtree(dest)
    os.remove(archive+".zip")

def mac_staple():
    if (platform.system() != "Darwin"): return
    externals = os.listdir("./externals")
    mac_externals =  [ s for s in externals if re.search(r"\.darwin-fat-32.so$", s) ]
    for external in mac_externals:
        cmd = f'xcrun stapler staple ./externals/{external}'
        p = subprocess.Popen(cmd, shell=True)
        p.wait()  

def mac_validate():
    if (platform.system() != "Darwin"): return
    externals = os.listdir("./externals")
    mac_externals =  [ s for s in externals if re.search(r"\.darwin-fat-32.so$", s) ]
    for external in mac_externals:
        print(f"===== {external} ======")
        cmd = f'codesign -dv --verbose=4 ./externals/{external}'
        p = subprocess.Popen(cmd, shell=True)  
        p.wait()
        print("\n")

def main():
    macos_codesign()
    mac_staple()
    mac_validate()




if __name__=="__main__":
    main() 
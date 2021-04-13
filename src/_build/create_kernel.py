import hashlib
import os
import re
import subprocess
import sys



FILE_HASH_BUF_SIZE=65536
INCLUDE_LIST_REGEX=re.compile(br"^\s*?((?:\s*#\s*include\s*<[^>]*?>)+)",re.M)
INCLUDE_FILE_REGEX=re.compile(br"^\s*#\s*include\s*<([^>]*?)>$")
REQUIRED_STRUCTURE_OFFSETS={}
REQUIRED_STRUCTURE_SIZE=[]
REQUIRED_DEFINITIONS=[]
REQUIRED_TYPE_SIZE=[]
SIZEOF_POINTER=8
SIZEOF_UINT8_T=1
SIZEOF_UINT16_T=2
SIZEOF_UINT32_T=4
SIZEOF_UINT64_T=8
with open("src/_build/exports.txt","rb") as f:
	c=None
	c_s=None
	c_s_i=None
	for k in f.read().replace(b"\r\n",b"\n").split(b"\n"):
		c_i=b""
		while (k[:1] in b" \t\r\n\v\f"):
			c_i+=k[:1]
			k=k[1:]
			if (len(k)==0):
				break
		if (len(k)==0):
			break
		if (len(c_i)==0 and k in [b"structures:",b"defs:",b"sizes:"]):
			c=k[:-1]
		else:
			if (c==b"structures"):
				if (c_s is None or len(c_i)==len(c_s_i)):
					c_s=k[:-1]
					c_s_i=c_i
				else:
					if (k==b"$$size$$"):
						REQUIRED_STRUCTURE_SIZE.append(c_s)
					else:
						if (c_s not in REQUIRED_STRUCTURE_OFFSETS):
							REQUIRED_STRUCTURE_OFFSETS[c_s]=[k]
						else:
							REQUIRED_STRUCTURE_OFFSETS[c_s].append(k)
			elif (c==b"defs"):
				REQUIRED_DEFINITIONS.append(k)
			else:
				REQUIRED_TYPE_SIZE.append(k)



def create_kernel(dbg):
	print("Createing Kernel\x1b[38;2;100;100;100m...\x1b[0m")
	def _sort_inc(m,il):
		l=[INCLUDE_FILE_REGEX.search(e.strip()).group(1) for e in m.group(1).strip().split(b"\n")]
		il.extend(l)
		return (b"#include <shared.h>"+(b"\n" if len(l)>1 else b"") if b"shared.h" in l else b"")+b"\n".join([b"#include <"+e+b">" for e in sorted(l) if e!=b"shared.h"])
	def _check_new(f_h_dt,n_f_h_dt,f_inc,fp,o_fp):
		if (fp not in f_h_dt):
			return "First Compilation"
		if (f_h_dt[fp]!=n_f_h_dt[fp]):
			return "Source Change"
		if (not os.path.exists(o_fp)):
			return "Compiled File Missing"
		if (fp not in f_inc):
			return None
		for k in f_inc[fp]:
			if (k not in f_h_dt or f_h_dt[k]!=n_f_h_dt[k]):
				return f"Dependency Change: \x1b[38;2;225;125;40m{k}"
		return None
	print("  Generating Directories\x1b[38;2;100;100;100m...\x1b[0m")
	if (not os.path.exists("build")):
		os.mkdir("build")
	f_h_dt={}
	if (os.path.exists("build/__last_build_files__")):
		with open("build/__last_build_files__","r") as f:
			for k in f.read().split("\n"):
				k=k.strip()
				if (len(k)<33):
					continue
				f_h_dt[k[:-32]]=k[-32:]
	if (not os.path.exists("build/efi")):
		os.mkdir("build/efi")
	if (not os.path.exists("build/kernel")):
		os.mkdir("build/kernel")
	if (not os.path.exists("build/libc")):
		os.mkdir("build/libc")
	src_fl=list(os.walk("src"))+list(os.walk("rsrc"))
	asm_d=[]
	fd=[]
	f_inc={}
	n_f_h_dt={}
	asm_d_fl={}
	for k,v in REQUIRED_STRUCTURE_OFFSETS.items():
		for e in v:
			asm_d_fl[f"__C_{str(k,'utf-8').strip('_').upper()}_STRUCT_{str(e,'utf-8').upper()}_OFFSET__"]=[[],[]]
	for k in REQUIRED_STRUCTURE_SIZE:
		asm_d_fl[f"__C_{str(k,'utf-8').strip('_').upper()}_STRUCT_SIZE__"]=[[],[]]
	for k in REQUIRED_DEFINITIONS:
		asm_d_fl[f"__C_{str(k,'utf-8').strip('_').upper()}__"]=[[],[]]
	err=False
	for k in REQUIRED_TYPE_SIZE:
		if (k==b"uint32_t"):
			asm_d+=[f"-D__C_SIZEOF_UINT32_T__={SIZEOF_UINT32_T}"]
		elif (k==b"uint64_t"):
			asm_d+=[f"-D__C_SIZEOF_UINT64_T__={SIZEOF_UINT64_T}"]
		else:
			err=True
			print(f"Unknown Size of Type '{str(k,'utf-8')}'!")
	if (err):
		sys.exit(1)
	print("  Prettyfying Files\x1b[38;2;100;100;100m...\x1b[0m")
	for r,_,fl in src_fl:
		r=r.replace("\\","/")+"/"
		for f in fl:
			if (f[-2:]==".c" or f[-2:]==".h"):
				print(f"    Prettifying \x1b[38;2;185;65;190mC {('Source' if f[-1]=='c' else 'Header')}\x1b[0m File: \x1b[38;2;130;190;50m{r+f}\x1b[38;2;100;100;100m...\x1b[0m")
				with open(r+f,"rb") as rf:
					il=[]
					dt=INCLUDE_LIST_REGEX.sub(lambda m:_sort_inc(m,il),rf.read())
					f_inc[r+f]=[]
					for e in il:
						e=str(e.replace(b"\\",b"/").split(b"/")[-1],"utf-8")
						for tr,_,tfl in src_fl:
							tr=tr.replace("\\","/")+"/"
							for tf in tfl:
								if ((tr+tf).replace("\\","/").split("/")[-1]==e):
									f_inc[r+f]+=[(tr+tf)]
				h=hashlib.md5()
				h.update(dt)
				n_f_h_dt[r+f]=h.hexdigest()
				if (f[-1]=="h"):
					for k in set(REQUIRED_STRUCTURE_OFFSETS.keys())|set(REQUIRED_STRUCTURE_SIZE):
						m=re.search(br"struct\s+"+k+br"\s*\{",dt)
						if (m is not None):
							i=m.end()
							b=1
							while (b>0 or dt[i:i+1] in b" \t\r\n\v\f"):
								if (dt[i:i+1]==b"{"):
									b+=1
								elif (dt[i:i+1]==b"}"):
									b-=1
								i+=1
							p=False
							if (dt[i:i+15]==b"__attribute__((" and dt[i+15:].split(b")")[0].strip(b"_")==b"packed"):
								p=True
							i=m.end()
							off=0
							lc=-1
							while (True):
								if (dt[i:i+1] in b" \t\r\n\v\f"):
									i+=1
									continue
								e=b""
								while (dt[i:i+1]!=b";"):
									e+=dt[i:i+1]
									i+=1
								i+=1
								t=e[:-len(e.split(b" ")[-1])].strip()
								c=0
								if (t[:1]==b"}"):
									break
								elif (t[-1:]==b"*"):
									c=SIZEOF_POINTER
								elif (t==b"uint8_t"):
									c=SIZEOF_UINT8_T
								elif (t==b"uint16_t"):
									c=SIZEOF_UINT16_T
								elif (t==b"uint32_t"):
									c=SIZEOF_UINT32_T
								elif (t==b"uint64_t"):
									c=SIZEOF_UINT64_T
								else:
									print(f"Unknown sizeof of Type '{str(t,'utf-8')}'!")
									sys.exit(1)
								if (p==False and lc!=-1 and lc<c and off%c!=0):
									off+=c-off%c
								if (k in REQUIRED_STRUCTURE_OFFSETS):
									nm=e.split(b" ")[-1].replace(b"[]",b"")
									if (nm in REQUIRED_STRUCTURE_OFFSETS[k]):
										asm_d+=[f"-D__C_{str(k,'utf-8').strip('_').upper()}_STRUCT_{str(nm,'utf-8').upper()}_OFFSET__={off}"]
										asm_d_fl[f"__C_{str(k,'utf-8').strip('_').upper()}_STRUCT_{str(nm,'utf-8').upper()}_OFFSET__"][0]+=[r+f]
										REQUIRED_STRUCTURE_OFFSETS[k].remove(nm)
										if (len(REQUIRED_STRUCTURE_OFFSETS[k])==0 and k not in REQUIRED_STRUCTURE_SIZE):
											break
								off+=c
								lc=c
							if (k in REQUIRED_STRUCTURE_SIZE):
								asm_d+=[f"-D__C_{str(k,'utf-8').strip('_').upper()}_STRUCT_SIZE__={off}"]
								asm_d_fl[f"__C_{str(k,'utf-8').strip('_').upper()}_STRUCT_SIZE__"][0]+=[r+f]
								REQUIRED_STRUCTURE_SIZE.remove(k)
							if (k in REQUIRED_STRUCTURE_OFFSETS):
								if (REQUIRED_STRUCTURE_OFFSETS[k]):
									for e in REQUIRED_STRUCTURE_OFFSETS[k]:
										print(f"Unable to Find Element '{str(e,'utf-8')}' in Structure '{str(k,'utf-8')}'!")
									sys.exit(1)
								del REQUIRED_STRUCTURE_OFFSETS[k]
				for k in REQUIRED_DEFINITIONS:
					l=re.findall(br"\#define\s+"+k+br"\s*(.*)$",dt,re.M)
					if (len(l)==0):
						continue
					if (k not in fd and len(l)==1):
						asm_d+=[f"-D__C_{str(k,'utf-8').strip('_').upper()}__={str(l[0].strip(),'utf-8')}"]
						asm_d_fl[f"__C_{str(k,'utf-8').strip('_').upper()}__"][0]+=[r+f]
						fd+=[k]
					else:
						print(f"Found Multiple Preprocessor Definitions of '{str(k,'utf-8')}!")
						sys.exit(1)
				with open(r+f,"wb") as wf:
					wf.write(dt)
			elif (f[-4:]==".asm"):
				dt=b""
				with open(r+f,"rb") as rf:
					h=hashlib.md5()
					while (True):
						c=rf.read(FILE_HASH_BUF_SIZE)
						dt+=c
						if (len(c)==0):
							break
						h.update(c)
					n_f_h_dt[r+f]=h.hexdigest()
				dt=str(dt,"utf-8")
				for k in asm_d_fl:
					if (k in dt):
						asm_d_fl[k][1]+=[r+f]
				f_inc[r+f]=[]
	for k in REQUIRED_STRUCTURE_OFFSETS.keys():
		print(f"Unable to Find Struture '{str(k,'utf-8')}'!")
	for k in REQUIRED_STRUCTURE_SIZE:
		print(f"Unable to Find Struture '{str(k,'utf-8')}'!")
	err=False
	for k,v in asm_d_fl.items():
		if (len(v[1])==0):
			print(f"External Definition '{k}' not used in Assembly!")
			err=True
		else:
			for e in v[1]:
				for se in v[0]:
					if (se not in f_inc[e]):
						f_inc[e]+=[se]
	if (REQUIRED_STRUCTURE_OFFSETS or REQUIRED_STRUCTURE_SIZE or err):
		sys.exit(1)
	while (True):
		u=False
		for k,v in f_inc.items():
			for e in v:
				for se in f_inc[e]:
					if (se not in v):
						u=True
						v.append(se)
		if (not u):
			break
	e_fl=[]
	k_fl=[]
	l_fl=[]
	cl={k:(v if k[-2:]!=".h" else n_f_h_dt[k]) for k,v in f_h_dt.items() if k in n_f_h_dt}
	ed=([] if not dbg else ["-D__DEBUG_BUILD__=1"])
	print("  Compiling Files\x1b[38;2;100;100;100m...\x1b[0m")
	for r,_,fl in src_fl:
		r=r.replace("\\","/")+"/"
		for f in fl:
			if (r[:8]=="src/uefi"):
				if (f[-2:]==".c"):
					ss=_check_new(f_h_dt,n_f_h_dt,f_inc,r+f,f"build/efi/{(r+f).replace('/','$')}.o")
					if (ss is not None):
						print(f"    Compiling \x1b[38;2;185;65;190mC Efi File\x1b[0m: \x1b[38;2;130;190;50m{r+f} \x1b[0m-> \x1b[38;2;240;240;40mbuild/efi/{(r+f).replace('/','$')}.o \x1b[0m(\x1b[38;2;80;140;210m{ss}\x1b[0m)\x1b[38;2;100;100;100m...\x1b[0m")
						if (subprocess.run(["bash","-c",f"gcc -Isrc/kernel/include -I/usr/include/efi -I/usr/include/efi/x86_64 -I/usr/include/efi/protocol -fno-stack-protector -O3 -fpic -fshort-wchar -fno-common -mno-red-zone -D__UEFI_LOADER__=1 -DHAVE_USE_MS_ABI{('' if not dbg else ' -D__DEBUG_BUILD__=1')} -Wall -Werror -c {r+f} -o build/efi/{(r+f).replace('/',chr(92)+'$')}.o"]).returncode!=0 or subprocess.run(["strip","-R",".rdata$zzz","--keep-file-symbols","--strip-debug","--strip-unneeded","--discard-locals",f"build/efi/{(r+f).replace('/','$')}.o"]).returncode!=0):
							print("      Writing File Hash List\x1b[38;2;100;100;100m...\x1b[0m")
							with open("build/__last_build_files__","w") as f:
								for k,v in cl.items():
									f.write(f"{k}{v}\n")
							sys.exit(1)
					else:
						print(f"    Already Compiled: (\x1b[38;2;185;65;190mC Efi File\x1b[0m): \x1b[38;2;130;190;50m{r+f} \x1b[0m-> \x1b[38;2;240;240;40mbuild/efi/{(r+f).replace('/','$')}.o\x1b[0m")
					e_fl+=[f"build/efi/{(r+f).replace('/','$')}.o"]
					cl[r+f]=n_f_h_dt[r+f]
				elif (f[-4:]==".asm"):
					ss=_check_new(f_h_dt,n_f_h_dt,f_inc,r+f,f"build/efi/{(r+f).replace('/','$')}.o")
					if (ss is not None):
						print(f"    Compiling \x1b[38;2;185;65;190mASM Efi File\x1b[0m: \x1b[38;2;130;190;50m{r+f} \x1b[0m-> \x1b[38;2;240;240;40mbuild/efi/{(r+f).replace('/','$')}.o \x1b[0m(\x1b[38;2;80;140;210m{ss}\x1b[0m)\x1b[38;2;100;100;100m...\x1b[0m")
						if (subprocess.run(["nasm",r+f,"-f","elf64","-O3","-D__UEFI_LOADER__=1","-Wall","-Werror","-o",f"build/efi/{(r+f).replace('/','$')}.o"]+asm_d+ed).returncode!=0):
							print("      Writing File Hash List\x1b[38;2;100;100;100m...\x1b[0m")
							with open("build/__last_build_files__","w") as f:
								for k,v in cl.items():
									f.write(f"{k}{v}\n")
							sys.exit(1)
					else:
						print(f"    Already Compiled: (\x1b[38;2;185;65;190mASM Efi File\x1b[0m): \x1b[38;2;130;190;50m{r+f} \x1b[0m-> \x1b[38;2;240;240;40mbuild/efi/{(r+f).replace('/','$')}.o\x1b[0m")
					e_fl+=[f"build/efi/{(r+f).replace('/','$')}.o"]
					cl[r+f]=n_f_h_dt[r+f]
			elif (r[:10]=="src/kernel"):
				if (f[-2:]==".c"):
					ss=_check_new(f_h_dt,n_f_h_dt,f_inc,r+f,f"build/kernel/{(r+f).replace('/','$')}.o")
					if (ss is not None):
						print(f"    Compiling \x1b[38;2;185;65;190mC Kernel File\x1b[0m: \x1b[38;2;130;190;50m{r+f} \x1b[0m-> \x1b[38;2;240;240;40mbuild/kernel/{(r+f).replace('/','$')}.o \x1b[0m(\x1b[38;2;80;140;210m{ss}\x1b[0m)\x1b[38;2;100;100;100m...\x1b[0m")
						if (subprocess.run(["gcc","-mcmodel=large","-mno-red-zone","-fno-common","-m64","-Wall","-Werror","-fpic","-ffreestanding","-fno-stack-protector","-O3","-nostdinc","-nostdlib","-c",r+f,"-o",f"build/kernel/{(r+f).replace('/','$')}.o","-Isrc/kernel/include","-Irsrc/include","-Isrc/libc/include","-Irsrc"]+ed).returncode!=0 or subprocess.run(["strip","-R",".rdata$zzz","--keep-file-symbols","--strip-debug","--strip-unneeded","--discard-locals",f"build/kernel/{(r+f).replace('/','$')}.o"]).returncode!=0):
							print("      Writing File Hash List\x1b[38;2;100;100;100m...\x1b[0m")
							with open("build/__last_build_files__","w") as f:
								for k,v in cl.items():
									f.write(f"{k}{v}\n")
							sys.exit(1)
					else:
						print(f"    Already Compiled: (\x1b[38;2;185;65;190mC Kernel File\x1b[0m): \x1b[38;2;130;190;50m{r+f} \x1b[0m-> \x1b[38;2;240;240;40mbuild/kernel/{(r+f).replace('/','$')}.o\x1b[0m")
					k_fl+=[f"build/kernel/{(r+f).replace('/','$')}.o"]
					cl[r+f]=n_f_h_dt[r+f]
				elif (f[-4:]==".asm"):
					ss=_check_new(f_h_dt,n_f_h_dt,f_inc,r+f,f"build/kernel/{(r+f).replace('/','$')}.o")
					if (ss is not None):
						print(f"    Compiling \x1b[38;2;185;65;190mASM Kernel File\x1b[0m: \x1b[38;2;130;190;50m{r+f} \x1b[0m-> \x1b[38;2;240;240;40mbuild/kernel/{(r+f).replace('/','$')}.o \x1b[0m(\x1b[38;2;80;140;210m{ss}\x1b[0m)\x1b[38;2;100;100;100m...\x1b[0m")
						if (subprocess.run(["nasm",r+f,"-f","elf64","-O3","-Wall","-Werror","-o",f"build/kernel/{(r+f).replace('/','$')}.o"]+asm_d+ed).returncode!=0):
							print("      Writing File Hash List\x1b[38;2;100;100;100m...\x1b[0m")
							with open("build/__last_build_files__","w") as f:
								for k,v in cl.items():
									f.write(f"{k}{v}\n")
							sys.exit(1)
					else:
						print(f"    Already Compiled: (\x1b[38;2;185;65;190mASM Kernel File\x1b[0m): \x1b[38;2;130;190;50m{r+f} \x1b[0m-> \x1b[38;2;240;240;40mbuild/kernel/{(r+f).replace('/','$')}.o\x1b[0m")
					k_fl+=[f"build/kernel/{(r+f).replace('/','$')}.o"]
					cl[r+f]=n_f_h_dt[r+f]
			elif (r[:4]=="rsrc"):
				if (f[-2:]==".c"):
					ss=_check_new(f_h_dt,n_f_h_dt,f_inc,r+f,f"build/kernel/{(r+f).replace('/','$')}.o")
					if (ss is not None):
						print(f"    Compiling \x1b[38;2;185;65;190mC Kernel Resource File\x1b[0m: \x1b[38;2;130;190;50m{r+f} \x1b[0m-> \x1b[38;2;240;240;40mbuild/kernel/{(r+f).replace('/','$')}.o \x1b[0m(\x1b[38;2;80;140;210m{ss}\x1b[0m)\x1b[38;2;100;100;100m...\x1b[0m")
						if (subprocess.run(["gcc","-mcmodel=large","-mno-red-zone","-fno-common","-m64","-Wall","-Werror","-fpic","-ffreestanding","-fno-stack-protector","-O3","-nostdinc","-nostdlib","-c",r+f,"-o",f"build/kernel/{(r+f).replace('/','$')}.o","-Irsrc/include","-Isrc/kernel/include","-Isrc/libc/include","-Irsrc"]+ed).returncode!=0 or subprocess.run(["strip","-R",".rdata$zzz","--keep-file-symbols","--strip-debug","--strip-unneeded","--discard-locals",f"build/kernel/{(r+f).replace('/','$')}.o"]).returncode!=0):
							print("      Writing File Hash List\x1b[38;2;100;100;100m...\x1b[0m")
							with open("build/__last_build_files__","w") as f:
								for k,v in cl.items():
									f.write(f"{k}{v}\n")
							sys.exit(1)
					else:
						print(f"    Already Compiled: (\x1b[38;2;185;65;190mC Kernel Resource File\x1b[0m): \x1b[38;2;130;190;50m{r+f} \x1b[0m-> \x1b[38;2;240;240;40mbuild/kernel/{(r+f).replace('/','$')}.o\x1b[0m")
					k_fl+=[f"build/kernel/{(r+f).replace('/','$')}.o"]
					cl[r+f]=n_f_h_dt[r+f]
			elif (r[:8]=="src/libc"):
				if (f[-2:]==".c"):
					ss=_check_new(f_h_dt,n_f_h_dt,f_inc,r+f,f"build/kernel/{(r+f).replace('/','$')}.o")
					if (ss is not None):
						print(f"    Compiling \x1b[38;2;185;65;190mC Kernel LibC File\x1b[0m: \x1b[38;2;130;190;50m{r+f} \x1b[0m-> \x1b[38;2;240;240;40mbuild/kernel/{(r+f).replace('/','$')}.o \x1b[0m(\x1b[38;2;80;140;210m{ss}\x1b[0m)\x1b[38;2;100;100;100m...\x1b[0m")
						if (subprocess.run(["gcc","-mcmodel=large","-mno-red-zone","-fno-common","-m64","-Wall","-Werror","-fpic","-ffreestanding","-fno-stack-protector","-O3","-nostdinc","-nostdlib","-c",r+f,"-o",f"build/kernel/{(r+f).replace('/','$')}.o","-Isrc/kernel/include","-Isrc/libc/include","-Irsrc","-D__KERNEL__=1"]+ed).returncode!=0 or subprocess.run(["strip","-R",".rdata$zzz","--keep-file-symbols","--strip-debug","--strip-unneeded","--discard-locals",f"build/kernel/{(r+f).replace('/','$')}.o"]).returncode!=0):
							print("      Writing File Hash List\x1b[38;2;100;100;100m...\x1b[0m")
							with open("build/__last_build_files__","w") as f:
								for k,v in cl.items():
									f.write(f"{k}{v}\n")
							sys.exit(1)
					else:
						print(f"    Already Compiled: (\x1b[38;2;185;65;190mC Kernel LibC File\x1b[0m): \x1b[38;2;130;190;50m{r+f} \x1b[0m-> \x1b[38;2;240;240;40mbuild/kernel/{(r+f).replace('/','$')}.o\x1b[0m")
					k_fl+=[f"build/kernel/{(r+f).replace('/','$')}.o"]
					ss=_check_new(f_h_dt,n_f_h_dt,f_inc,r+f,f"build/libc/{(r+f).replace('/','$')}.o")
					if (ss is not None):
						print(f"    Compiling \x1b[38;2;185;65;190mC LibC File\x1b[0m: \x1b[38;2;130;190;50m{r+f} \x1b[0m-> \x1b[38;2;240;240;40mbuild/libc/{(r+f).replace('/','$')}.o \x1b[0m(\x1b[38;2;80;140;210m{ss}\x1b[0m)\x1b[38;2;100;100;100m...\x1b[0m")
						if (subprocess.run(["gcc","-mcmodel=large","-mno-red-zone","-fno-common","-m64","-Wall","-Werror","-fpic","-ffreestanding","-fno-stack-protector","-O3","-nostdinc","-nostdlib","-c",r+f,"-o",f"build/libc/{(r+f).replace('/','$')}.o","-Isrc/libc/include","-Irsrc"]+ed).returncode!=0 or subprocess.run(["strip","-R",".rdata$zzz","--keep-file-symbols","--strip-debug","--strip-unneeded","--discard-locals",f"build/libc/{(r+f).replace('/','$')}.o"]).returncode!=0):
							print("      Writing File Hash List\x1b[38;2;100;100;100m...\x1b[0m")
							with open("build/__last_build_files__","w") as f:
								for k,v in cl.items():
									f.write(f"{k}{v}\n")
							sys.exit(1)
					else:
						print(f"    Already Compiled: (\x1b[38;2;185;65;190mC LibC File\x1b[0m): \x1b[38;2;130;190;50m{r+f} \x1b[0m-> \x1b[38;2;240;240;40mbuild/libc/{(r+f).replace('/','$')}.o\x1b[0m")
					l_fl+=[f"build/libc/{(r+f).replace('/','$')}.o"]
					cl[r+f]=n_f_h_dt[r+f]
				elif (f[-4:]==".asm"):
					ss=_check_new(f_h_dt,n_f_h_dt,f_inc,r+f,f"build/kernel/{(r+f).replace('/','$')}.o")
					if (ss is not None):
						print(f"    Compiling \x1b[38;2;185;65;190mASM Kernel LibC File\x1b[0m: \x1b[38;2;130;190;50m{r+f} \x1b[0m-> \x1b[38;2;240;240;40mbuild/kernel/{(r+f).replace('/','$')}.o \x1b[0m(\x1b[38;2;80;140;210m{ss}\x1b[0m)\x1b[38;2;100;100;100m...\x1b[0m")
						if (subprocess.run(["nasm",r+f,"-f","elf64","-O3","-Wall","-Werror","-D__KERNEL__=1","-o",f"build/kernel/{(r+f).replace('/','$')}.o"]+ed).returncode!=0):
							print("      Writing File Hash List\x1b[38;2;100;100;100m...\x1b[0m")
							with open("build/__last_build_files__","w") as f:
								for k,v in cl.items():
									f.write(f"{k}{v}\n")
							sys.exit(1)
					else:
						print(f"    Already Compiled: (\x1b[38;2;185;65;190mASM Kernel LibC File\x1b[0m): \x1b[38;2;130;190;50m{r+f} \x1b[0m-> \x1b[38;2;240;240;40mbuild/kernel/{(r+f).replace('/','$')}.o\x1b[0m")
					k_fl+=[f"build/kernel/{(r+f).replace('/','$')}.o"]
					ss=_check_new(f_h_dt,n_f_h_dt,f_inc,r+f,f"build/libc/{(r+f).replace('/','$')}.o")
					if (ss is not None):
						print(f"    Compiling \x1b[38;2;185;65;190mASM LibC File\x1b[0m: \x1b[38;2;130;190;50m{r+f} \x1b[0m-> \x1b[38;2;240;240;40mbuild/libc/{(r+f).replace('/','$')}.o \x1b[0m(\x1b[38;2;80;140;210m{ss}\x1b[0m)\x1b[38;2;100;100;100m...\x1b[0m")
						if (subprocess.run(["nasm",r+f,"-f","elf64","-O3","-Wall","-Werror","-o",f"build/libc/{(r+f).replace('/','$')}.o"]+ed).returncode!=0):
							print("      Writing File Hash List\x1b[38;2;100;100;100m...\x1b[0m")
							with open("build/__last_build_files__","w") as f:
								for k,v in cl.items():
									f.write(f"{k}{v}\n")
							sys.exit(1)
					else:
						print(f"    Already Compiled: (\x1b[38;2;185;65;190mASM LibC File\x1b[0m): \x1b[38;2;130;190;50m{r+f} \x1b[0m-> \x1b[38;2;240;240;40mbuild/libc/{(r+f).replace('/','$')}.o\x1b[0m")
					l_fl+=[f"build/libc/{(r+f).replace('/','$')}.o"]
					cl[r+f]=n_f_h_dt[r+f]
	print("  Writing File Hash List\x1b[38;2;100;100;100m...\x1b[0m")
	with open("build/__last_build_files__","w") as f:
		for k,v in n_f_h_dt.items():
			f.write(f"{k}{v}\n")
	print(f"  Linking UEFI OS Loader:\n    \x1b[38;2;130;190;50m{(chr(10)+'    ').join(e_fl)}\x1b[0m")
	if (subprocess.run(["bash","-c",f"ld -nostdlib -znocombreloc -fshort-wchar -T /usr/lib/elf_x86_64_efi.lds -shared -Bsymbolic -L /usr/lib /usr/lib/crt0-efi-x86_64.o {' '.join([e.replace('$',chr(92)+'$') for e in e_fl])} -o build/loader.efi -lefi -lgnuefi"]).returncode==0 and subprocess.run(["objcopy","-j",".text","-j",".sdata","-j",".data","-j",".dynamic","-j",".dynsym","-j",".rel","-j",".rela","-j",".reloc","--target=efi-app-x86_64","build/loader.efi","build/loader.efi"]).returncode!=0 or subprocess.run(["strip","-s","build/loader.efi"]).returncode!=0):
		sys.exit(1)
	print(f"  Linking Kernel:\n    \x1b[38;2;130;190;50m{(chr(10)+'    ').join(k_fl)}\x1b[0m")
	if (subprocess.run(["ld","-melf_x86_64","-o","build/kernel.elf","-s","-T","src/_build/kernel.ld","--oformat","elf64-x86-64"]+k_fl).returncode!=0):
		sys.exit(1)
	if (dbg):
		print(f"    Linking Debug Kernel:\n    \x1b[38;2;130;190;50m{(chr(10)+'    ').join(k_fl)}\x1b[0m")
		if (subprocess.run(["ld","-melf_x86_64","-o","build/dbg_kernel.elf","-T","src/_build/kernel.ld","--oformat","elf64-x86-64"]+k_fl).returncode!=0):
			sys.exit(1)
	print(f"  Linking LibC:\n    \x1b[38;2;130;190;50m{(chr(10)+'    ').join(l_fl)}\x1b[0m")
	if (subprocess.run(["ld","-melf_x86_64","-o","build/libc.so","-s","-shared","-flinker-output=pie","--oformat","elf64-x86-64"]+l_fl).returncode!=0):
		sys.exit(1)
	print("  Creaing OS Image\x1b[38;2;100;100;100m...\x1b[0m")
	if (subprocess.run(["dd","if=/dev/zero","of=build/os.img","bs=512","count=93750"]).returncode!=0 or subprocess.run(["bash","-c","parted build/os.img -s -a minimal mklabel gpt&&parted build/os.img -s -a minimal mkpart UEFI FAT32 2048s 93716s&&parted build/os.img -s -a minimal toggle 1 boot&&dd if=/dev/zero of=build/tmp.img bs=512 count=91669&&mformat -i build/tmp.img -h 32 -t 32 -n 64 -c 1&&mmd -i build/tmp.img ::/UEFI ::/UEFI/BOOT ::/os&&mcopy -i build/tmp.img build/kernel.elf ::/KERNEL.ELF&&mcopy -i build/tmp.img build/loader.efi ::/UEFI/BOOT/BOOTX64.UEFI&&mcopy -i build/tmp.img build/libc.so ::/os/libc.so&&dd if=build/tmp.img of=build/os.img bs=512 count=91669 seek=2048 conv=notrunc"]).returncode!=0):
		sys.exit(1)
	print("  Cleaning Up\x1b[38;2;100;100;100m...\x1b[0m")
	os.remove("build/loader.efi")
	os.remove("build/kernel.elf")
	os.remove("build/libc.so")
	os.remove("build/tmp.img")

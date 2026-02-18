# tidy
s/^x86_64-elf-//;
s/-m[a-z0-9-][a-z0-9-]* *//g;
s/-W[a-z0-9-]* *//g;
s/-f[a-z0-9-]* *//g;
s/-M[DP] *//g;
s/-MF *[^ ]* *//g;
s/-std=[a-z0-9]* *//g;
/^mkdir /d;
/^touch /d;
/^nm /d;
/^objdump /d;
# strip more command-line options
s/-nostdinc *//;
s/-nostdlib *//;
s/-static *//;
s/-nostartfiles *//;
s/-m elf_x86_64 *//;
s#link/[a-z0-9.]*.ld *##g;
s/-T *//;
s/-D[A-Za-z0-9=_]* *//g;
s/-O[0-4s]* *//;
s/-gdwarf-2 *//;
s/--gc-sections *//;
s/-z max-page-size=0x[A-Fa-f0-9]* *//;
s#-I[.a-zA-Z0-9/]* *##g;
s#-S binary *##;
s#-b binary *##;
s/-j \.[a-z]* *//g;
#convert
s/  */ /g;
/^cc -o obj\/mkbootdisk/d;
s/^cc -o \([^ ]*\) \(.*\)$/cc: \2 -> \1/;
s/^gcc -c \([^ ]*\) -o \(.*\)$/gcc: \1 -> \2/;
s/^ld -o \([^ ]*\) \(.*\)$/ld: \2 -> \1/;
s/^objcopy \([^ ]*\) \(.*\)$/objcopy: \1 -> \2/;
s#^obj/mkbootdisk \([^ ]*\) \(.*\) > \(.*\)$#objcopy: \1 \2 -> \3#;


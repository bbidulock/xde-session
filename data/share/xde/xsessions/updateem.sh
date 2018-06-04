#!/bin/bash

# First get a list of files

# find all the xsessions files
yaourt -Fyl |grep 'usr/share/xsessions/.*\.desktop$'|sort >xsessions.lst

# get a list of packages containing them
cat xsessions.lst|awk '{print$1}'|sort -u >packages.lst

# download all the packages into the cache
yaourt -Syw --nodeps `cat packages.lst`

# where is the cache?
cachedir=$(yaourt -v|awk '/Cache Dirs/{print$NF;exit}')

[ -d "$cachedir" ] || { echo "Couldn't find cache" >&2; exit 1; }

while read -a tokens; do
	namevers=$(yaourt -Sp --print-format='%n-%v' ${tokens[0]})
	echo "Processing package $namevers" >&2
	for f in $cachedir$namevers-*.pkg.tar.xz ; do
		if [ ! -f "$f" ]; then
			echo "Cannot find package ${tokens[0]} file $f" >&2
			continue
		fi
		file="$(basename ${tokens[1]}).${tokens[0]}"
		echo "extracting ${tokens[1]} from $f into ${file}" >&2
		bsdtar -x -O -f $f ${tokens[1]} >${file}
		if yaourt -Sia aur/${tokens[0]} >/dev/null 2>&1 ; then
			repo=$(yaourt -Sia aur/${tokens[0]}|awk '/^Repository/{print$NF;exit}')
			name=$(yaourt -Sia aur/${tokens[0]}|awk '/^Name/{print$NF;exit}')
			vers=$(yaourt -Sia aur/${tokens[0]}|awk '/^Version/{print$NF;exit}')
			desc=$(yaourt -Sia aur/${tokens[0]}|awk '/^Description/{print$0;exit}'|sed 's,.*: ,,')
			home=$(yaourt -Sia aur/${tokens[0]}|awk '/^URL/{print$0;exit}'|sed 's,.*: ,,')
			grps=$(yaourt -Sia aur/${tokens[0]}|awk '/^Groups/{print$NF;exit}')
			popc=$(yaourt -Sia aur/${tokens[0]}|awk '/^Popularity/{print$NF;exit}')
			vote=$(yaourt -Sia aur/${tokens[0]}|awk '/^Votes/{print$NF;exit}')
		else
			repo=$(yaourt -Si ${tokens[0]}|awk '/^Repository/{print$NF;exit}')
			name=$(yaourt -Si ${tokens[0]}|awk '/^Name/{print$NF;exit}')
			vers=$(yaourt -Si ${tokens[0]}|awk '/^Version/{print$NF;exit}')
			desc=$(yaourt -Si ${tokens[0]}|awk '/^Description/{print$0;exit}'|sed 's,.*: ,,')
			home=$(yaourt -Si ${tokens[0]}|awk '/^URL/{print$0;exit}'|sed 's,.*: ,,')
			grps=$(yaourt -Si ${tokens[0]}|awk '/^Groups/{print$NF;exit}')
			popc="0"
			vote="0"
		fi
		{
			echo "X-AppInstall-Package=$name"
			echo "X-AppInstall-Popcon=$popc"
			echo "X-AppInstall-Section=$repo"
			echo "X-Arch-Package=$name $vers"
			echo "X-Arch-Package-Repository=$repo"
			echo "X-Arch-Package-Name=$name"
			echo "X-Arch-Package-Version=$vers"
			echo "X-Arch-Package-Description=$desc"
			echo "X-Arch-Package-Groups=$grps"
			echo "X-Arch-Package-URL=$home"
			echo "X-Arch-Package-Votes=$vote"
		} >>${file}
	done
done <xsessions.lst


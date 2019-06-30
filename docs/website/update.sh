#!/usr/bin/env zsh

typeset -A list


list=(mkdocs-dyne-theme https://github.com/dyne/mkdocs-dyne-theme)

## check if symlink then leave it as likely being developed
for i in ${(k)list}; do
	if [[ -L ${i} && -d ${i} ]]; then
		echo "symlink skipped: $i"
		continue
	else

		if [[ -d ${i} && -d ${i}/.git ]]; then
			pushd ${i} && git pull --rebase;
			popd
		elif [[ -d ${i} ]]; then
			echo "overwriting static dir: $i"
			rm -rf "${i}.bck" && mv "${i}" "${i}.bck"
			echo "old dir saved in: $i.bck"
			git clone ${list[$i]} ${i}
		fi
	fi
done

#git submodule foreach git pull --rebase


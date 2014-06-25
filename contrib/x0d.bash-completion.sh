# This file is part of the "x0" project, http://xzero.io/
#   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
#
# Licensed under the MIT License (the "License"); you may not use this
# file except in compliance with the License. You may obtain a copy of
# the License at: http://opensource.org/licenses/MIT

export _x0d_bashcomp_debug=0

_x0d_log() {
	if [[ $_x0d_bashcomp_debug -ne 0 ]]; then
		echo "${@}" >> /tmp/x0d.bash-completion.log
	fi
}

# _x0d_param(cmd: string, val: string, prefix: string = "")
_x0d_param() {
	local cmd="${1}"
	local val="${2}"
	local prefix="${3}"

	_x0d_log "x0d_param via: cmd:'${cmd}' val:'${val}' prefix:'${prefix}'"

	case "${cmd}" in
		-f|--config|-p|--pid-file|-l|--log-file)
			local files=( $(compgen -o filenames -f "${val}" ) )
			for file in "${files[@]}"; do
				if [[ -d ${file} ]]; then
					COMPREPLY=( "${COMPREPLY[@]}" "${prefix}${file}/" )
				else
					COMPREPLY=( "${COMPREPLY[@]}" "${prefix}${file} " )
				fi
			done
			return 0
			;;
		-i|--instant)
			local files=( $(compgen -d "${val}" ) )
			for file in "${files[@]}"; do
				if compgen -d "${file}/" &>/dev/null; then
					COMPREPLY=( "${COMPREPLY[@]}" "${prefix}${file}/" )
				else
					COMPREPLY=( "${COMPREPLY[@]}" "${prefix}${file} " )
				fi
			done
			return 0
			;;
		-o|--log-target)
			COMPREPLY=( $(compgen -P "${prefix}" -W "file console syslog systemd" -- "${val}" ) )
			COMPREPLY=( "${COMPREPLY[@]/%/ }" )
			return 0
			;;
		-s|--log-severity)
			COMPREPLY=( $(compgen -P "${prefix}" -W "emerg alert crit error warning notice info diag debug debug1 debug2 debug3" -- "${val}" ) )
			COMPREPLY=( "${COMPREPLY[@]/%/ }" )
			return 0
			;;
		-u|--user)
			COMPREPLY=( $(compgen -P "${prefix}" -u "${val}" ) )
			COMPREPLY=( "${COMPREPLY[@]/%/ }" )
			return 0
			;;
		-g|--group)
			COMPREPLY=( $(compgen -P "${prefix}" -g "${val}" ) )
			COMPREPLY=( "${COMPREPLY[@]/%/ }" )
			return 0
			;;
		-O|--optimization-level)
			COMPREPLY=( $(compgen -P "${prefix}" -W "0 1 2 3" -- "${val}" ) )
			COMPREPLY=( "${COMPREPLY[@]/%/ }" )
			return 0
			;;
	esac
	_x0d_log "lookup failed"
	return 1
}

_x0d() {
	local cur prev

	COMP_WORDBREAKS=" "
	COMPREPLY=()

	cur="${COMP_WORDS[COMP_CWORD]}"
	prev="${COMP_WORDS[COMP_CWORD - 1]}"

	_x0d_param "${prev}" "${cur}" && return

	case "${cur}" in
		--*=|--*=*)
			_x0d_param "${cur/=*/}" "${cur/*=/}" "${cur/=*/}=" && return
			;;
		-*)
			_x0d_param "${cur:0:2}" "${cur:2}" "${cur:0:2}" && return
			;;
		*)
			_x0d_log "cur did not match. '${cur}'"
			;;
	esac

	if [[ "${cur}" == -* ]] || [[ "${cur}" == "" ]]; then
		local replies=( $(compgen -W "-h -f -O -X -G -P -u -g -o -l -s -i -k -v -y -V \
					--help --config= --optimization-level= --no-fork --systemd \
					--guard --pid-file= --user= --group= --log-target= --log-file= \
					--log-severity= --instant= --crash-handler= --version \
                    --dump-ast --dump-ir --dump-tc \
					--copyright --splash --info" -- "${cur}") )
		for reply in "${replies[@]}"; do
			if [[ "${reply}" != *= ]]; then
				COMPREPLY=( "${COMPREPLY[@]}" "${reply} " )
			else
				COMPREPLY=( "${COMPREPLY[@]}" "${reply}" )
			fi
		done
		return 0
	fi
}

complete -F _x0d -o nospace x0d

#!/usr/bin/env bash

set -euxo pipefail

ci_install() {
	mkdir -p -- "ext"

	local project="STL" commit="2914b4301c59dc7ffc09d16ac6f7979fde2b7f2c"
	curl -sfL "https://github.com/microsoft/${project}/archive/${commit}.tar.gz" | {
		tar -C "ext" --strip-components=0 -xzf - "${project}-${commit}"
		env -C "ext" mv -- "${project}-${commit}" "msvc-stl"
	}
	env -C "ext/msvc-stl" patch --merge --binary -f -s -p1 -i "../patches/msvc-stl.patch"

	local gcc_version; gcc_version="$(g++ --version | sed -n 's/^g++ (\w\+ \([^\-]*\).*/\1/p')"
	local project="gcc"
	curl -sfL "https://github.com/gcc-mirror/${project}/archive/releases/gcc-${gcc_version}.tar.gz" | {
		tar -C "ext" --strip-components=1 -xzf - "${project}-releases-gcc-${gcc_version}/libstdc++-v3"
	}
	env -C "ext" patch --merge --binary -f -s -p1 -i "patches/libstdc++-v3.patch"

	local clang_version; clang_version="$(clang++ --version | sed -n 's/^clang version \([^\-]*\).*/\1/p')"
	curl -sfL "https://github.com/llvm/llvm-project/releases/download/llvmorg-${clang_version}/libcxx-${clang_version}.src.tar.xz" | {
		mkdir -p "ext/libcxx"
		tar -C "ext/libcxx" --strip-components=1 -xJf - "libcxx-${clang_version}.src"
	}

	local packages=()
	if ! command -v valgrind > /dev/null; then packages+=(valgrind); fi
	if ! command -v      g++ > /dev/null; then packages+=(gcc     ); fi
	if ! command -v  clang++ > /dev/null; then packages+=(clang   ); fi
	packages+=(libc++-dev libc++abi-dev)
	sudo apt-get install -qq -o=Dpkg::Use-Pty=0 -- "${packages[@]}"
}

ci_before_script() {
	local srcdir="ext/libstdc++-v3/testsuite/23_containers/vector"
	local findargs=(
		-not -name "pmr*.cc"
		-not -path "${srcdir}/40192.cc"
		-not -path "${srcdir}/bool/*"
		-not -path "${srcdir}/cons/89164_c++17.cc"
		-not -path "${srcdir}/cons/89164.cc"
		-not -path "${srcdir}/debug/*"
		-not -path "${srcdir}/modifiers/push_back/89416.cc"  # TODO: Patch test
		-not -path "${srcdir}/modifiers/swap/1.cc"  # TODO: Patch test
		-not -path "${srcdir}/profile/*"
		-not -path "${srcdir}/ext_pointer/explicit_instantiation/3.cc"
		-not -path "${srcdir}/requirements/explicit_instantiation/3.cc"

		### No main() in these files:
		-not -path "${srcdir}/allocator/construction.cc"
		-not -path "${srcdir}/allocator/noexcept.cc"
		-not -path "${srcdir}/range_access.cc"
		-not -path "${srcdir}/requirements/partial_specialization/1.cc"
		-not -path "${srcdir}/requirements/typedefs.cc"
		-not -path "${srcdir}/requirements/dr438/constructor.cc"
		-not -path "${srcdir}/requirements/explicit_instantiation/1_c++0x.cc"
		-not -path "${srcdir}/requirements/explicit_instantiation/2.cc"
		-not -path "${srcdir}/requirements/explicit_instantiation/4.cc"
		-not -path "${srcdir}/requirements/explicit_instantiation/1.cc"
		-not -path "${srcdir}/requirements/do_the_right_thing.cc"
		-not -path "${srcdir}/capacity/87544.cc"
		-not -path "${srcdir}/18604.cc"
		-not -path "${srcdir}/26412-1.cc"
		-not -path "${srcdir}/ext_pointer/explicit_instantiation/2.cc"
		-not -path "${srcdir}/ext_pointer/explicit_instantiation/1.cc"
		-not -path "${srcdir}/modifiers/emplace/const_iterator.cc"
		-not -path "${srcdir}/modifiers/push_back/89130.cc"
		-not -path "${srcdir}/modifiers/insert/const_iterator.cc"
		-not -path "${srcdir}/modifiers/erase/54577.cc"
		-not -path "${srcdir}/types/23767.cc"
		-not -path "${srcdir}/63500.cc"
		-not -path "${srcdir}/cons/moveable2.cc"
		-not -path "${srcdir}/cons/deduction.cc"
		-not -path "${srcdir}/cons/55977.cc"
		-not -path "${srcdir}/cons/noexcept_move_construct.cc"
		-not -path "${srcdir}/cons/87809.cc"
		-not -path "${srcdir}/52591.cc"
		-not -path "${srcdir}/58764.cc"
		-not -path "${srcdir}/26412-2.cc"
	)
	find "ext/msvc-stl/tests/tr1/tests/vector"             -xdev -name "*.cpp"                                        -exec  clang++                                                   -ggdb3 -std=c++20 -iquote "include" -isystem "ext/msvc-stl/tests/tr1/include"  -o "{}.out" "{}" \;
	find "ext/libcxx/test/std/containers/sequences/vector" -xdev -name "*.cpp"                  -not -name "*.fail.*" -exec  clang++ -stdlib=libc++                                    -ggdb3 -std=c++20 -iquote "include" -isystem "ext/libcxx/test/support"         -o "{}.out" "{}" \;
	find "${srcdir}"                                       -xdev -name "*.cc"  "${findargs[@]}" -not -name  "*_neg.*" -exec      g++ -include "src/vital/containers/libstdc++.inc.hpp" -ggdb3 -std=c++2a -iquote "include" -isystem "ext/libstdc++-v3/testsuite/util" -o "{}.out" "{}" \;
}

ci_script() {
	find "ext/msvc-stl/tests/tr1/tests/vector" "ext/libcxx/test/std/containers/sequences/vector" "ext/libstdc++-v3/testsuite/23_containers/vector" -xdev -name "*.out" -exec valgrind --leak-check=full --show-leak-kinds=definite --track-origins=yes -q "{}" \;
}

if [ 0 -lt "$#" ]; then
	if declare -f ci_"$1" > /dev/null; then
		ci_"$1" "${@:2}"
	fi
fi

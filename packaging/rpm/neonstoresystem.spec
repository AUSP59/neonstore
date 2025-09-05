Name: neonstoresystem
Version: 1.0.0
Release: 1%{?dist}
Summary: World-class C++ store management CLI and library
License: Apache-2.0
URL: https://github.com/YOUR_ORG/neonstoresystem
Source0: %{name}-%{version}.tar.gz

BuildRequires: cmake, gcc-c++, make
%description
A fast, portable C++20 CLI and library for product/order inventory.

%prep
%setup -q

%build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

%install
cmake --install build --prefix %{buildroot}/usr

%files
%license LICENSE
%doc README.md
/usr/bin/neonstore
/usr/lib*/cmake/NeonStoreSystem/*
/usr/include/neonstore/*

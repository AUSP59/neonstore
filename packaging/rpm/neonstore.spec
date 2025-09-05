Name: neonstore
Version: 2.16.0
Release: 1%{?dist}
Summary: Store management CLI and C++ library
License: Apache-2.0
URL: https://example.org/neonstore
Source0: %{name}-%{version}.tar.gz

BuildRequires: cmake, gcc-c++, make

%description
A zero-dependency local CLI and C++ library for inventory and orders.

%prep
%setup -q

%build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

%install
cmake --install build --prefix %{buildroot}/usr

%files
%license LICENSE
%doc README.md
/usr/bin/neonstore

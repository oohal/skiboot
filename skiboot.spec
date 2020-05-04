Name:		opal-prd
Version:	3000.0
Release:	1%{?dist}
Summary:	OPAL Processor Recovery Diagnostics Daemon

Group:		System Environment/Daemons
License:	ASL 2.0
URL:		http://github.com/open-power/skiboot
ExclusiveArch:	ppc64le

BuildRequires:	systemd

Requires:	systemd

Source0:	https://github.com/open-power/skiboot/archive/v%{version}.tar.gz

%description
This package provides a daemon to load and run the OpenPower firmware's
Processor Recovery Diagnostics binary. This is responsible for run time
maintenance of OpenPower Systems hardware.

%prep

%setup -q -n skiboot-%{version}

%build
OPAL_PRD_VERSION=%version make V=1 -C external/opal-prd

%install
make -C external/opal-prd install DESTDIR=%{buildroot} prefix=/usr

mkdir -p %{buildroot}%{_unitdir}
install -m 644 -p external/opal-prd/opal-prd.service %{buildroot}%{_unitdir}/opal-prd.service

%post
if [ $1 -eq 1 ] ; then
    # Initial installation
    /bin/systemctl enable opal-prd.service >/dev/null 2>&1 || :
    /bin/systemctl start opal-prd.service >/dev/null 2>&1 || :
fi

%preun
if [ $1 -eq 0 ] ; then
    # Package removal, not upgrade
    /bin/systemctl --no-reload disable opal-prd.service > /dev/null 2>&1 || :
    /bin/systemctl stop opal-prd.service > /dev/null 2>&1 || :
fi

%postun
systemctl daemon-reload >/dev/null 2>&1 || :
if [ "$1" -ge 1 ] ; then
    /bin/systemctl try-restart opal-prd.service >/dev/null 2>&1 || :
fi

%files
%doc README.md
%license LICENCE
%{_sbindir}/opal-prd
%{_unitdir}/opal-prd.service
%{_mandir}/man8/*

%changelog
* Mon May 04 2020 Oliver O'Halloran <oohall@gmail.com> - 3000.0
- Specfile changes for the NVDIMM aware opal-prd.

* Thu Mar 01 2018 Murilo Opsfelder Araujo <muriloo@linux.vnet.ibm.com> - 5.10-1
- Update to v5.10 release

* Tue Feb 09 2016 Vasant Hegde <hegdevasant@linux.vnet.ibm.com> - 5.1.13
- Update to latest upstream release

* Mon Nov 23 2015 Vasant Hegde <hegdevasant@linux.vnet.ibm.com> - 5.1.12
- initial upstream spec file

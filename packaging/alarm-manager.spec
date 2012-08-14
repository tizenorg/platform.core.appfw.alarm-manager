Name:       alarm-manager
Summary:    Alarm library
Version:    0.4.46
Release:    1
Group:      System/Libraries
License:    Apache License, Version 2.0
Source0:    %{name}-%{version}.tar.gz
Source101:  packaging/alarm-server.service
Source1001: packaging/alarm-manager.manifest 

Requires(post): /sbin/ldconfig
Requires(post): /usr/bin/systemctl
Requires(postun): /sbin/ldconfig
Requires(postun): /usr/bin/systemctl
Requires(preun): /usr/bin/systemctl

BuildRequires: pkgconfig(dbus-1)
BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(dbus-glib-1)
BuildRequires: pkgconfig(pmapi)
BuildRequires: pkgconfig(dlog)
BuildRequires: pkgconfig(heynoti)
BuildRequires: pkgconfig(aul)
BuildRequires: pkgconfig(bundle)
BuildRequires: pkgconfig(security-server)
BuildRequires: pkgconfig(db-util)
BuildRequires: pkgconfig(vconf)
BuildRequires: pkgconfig(tapi)
BuildRequires: pkgconfig(appsvc)

%description
Alarm Server and devel libraries

%package -n alarm-server
Summary:    Alarm server (devel)
Group:      Development/Libraries

%description -n alarm-server
Alarm Server


%package -n libalarm
Summary:    Alarm server libraries
Group:      Development/Libraries
Requires:   alarm-server = %{version}-%{release}

%description -n libalarm
Alarm server library


%package -n libalarm-devel
Summary:    Alarm server libraries(devel)
Group:      Development/Libraries
Requires:   libalarm = %{version}-%{release}


%description -n libalarm-devel
Alarm server library (devel)

%prep
%setup -q

%build
cp %{SOURCE1001} .

export LDFLAGS+=" -Wl,--rpath=%{_libdir} -Wl,--as-needed"

%autogen --disable-static

dbus-binding-tool --mode=glib-server --prefix=alarm_manager ./alarm_mgr.xml > ./include/alarm-skeleton.h
dbus-binding-tool --mode=glib-client --prefix=alarm_manager ./alarm_mgr.xml > ./include/alarm-stub.h
dbus-binding-tool --mode=glib-server --prefix=alarm_client ./alarm-expire.xml > ./include/alarm-expire-skeleton.h
dbus-binding-tool --mode=glib-client --prefix=alarm_client ./alarm-expire.xml > ./include/alarm-expire-stub.h

%configure --disable-static
make %{?jobs:-j%jobs}


%install
rm -rf %{buildroot}
%make_install

mkdir -p %{buildroot}/etc/init.d
install -m 755 alarm-server_run %{buildroot}/etc/init.d

mkdir -p %{buildroot}/%{_sysconfdir}/rc.d/rc3.d
mkdir -p %{buildroot}/%{_sysconfdir}/rc.d/rc5.d
ln -s ../init.d/alarm-server_run %{buildroot}/%{_sysconfdir}/rc.d/rc3.d/S80alarm-server
ln -s ../init.d/alarm-server_run %{buildroot}/%{_sysconfdir}/rc.d/rc5.d/S80alarm-server

install -d %{buildroot}%{_libdir}/systemd/user/tizen-middleware.target.wants
install -m0644 %{SOURCE101} %{buildroot}%{_libdir}/systemd/user/
ln -sf ../alarm-server.service %{buildroot}%{_libdir}/systemd/user/tizen-middleware.target.wants/alarm-server.service

install -d %{buildroot}%{_sysconfdir}/udev/rules.d
install -m0644 rules/90-alarm-server.rules %{buildroot}%{_sysconfdir}/udev/rules.d/90-alarm-server.rules


%preun
if [ $1 == 0 ]; then
    systemctl stop alarm-server.service
fi

%post
/sbin/ldconfig
if [ $1 == 1 ]; then
    systemctl restart alarm-server.service
fi

%postun
/sbin/ldconfig
systemctl daemon-reload


%files -n alarm-server
%manifest alarm-manager.manifest
%attr(0755,root,root) %{_bindir}/alarm-server
%attr(0755,root,root) %{_sysconfdir}/init.d/alarm-server_run
%attr(0755,root,root) %{_sysconfdir}/rc.d/rc3.d/S80alarm-server
%attr(0755,root,root) %{_sysconfdir}/rc.d/rc5.d/S80alarm-server
%config %{_sysconfdir}/udev/rules.d/90-alarm-server.rules
%{_libdir}/systemd/user/tizen-middleware.target.wants/alarm-server.service
%{_libdir}/systemd/user/alarm-server.service

%files -n libalarm
%manifest alarm-manager.manifest
%attr(0644,root,root) %{_libdir}/libalarm.so.0.0.0
%{_libdir}/libalarm.so.0


%files -n libalarm-devel
%manifest alarm-manager.manifest
%{_includedir}/*.h
%{_libdir}/pkgconfig/*.pc
%{_libdir}/libalarm.so

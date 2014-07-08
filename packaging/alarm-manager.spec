Name:           alarm-manager
Version:        0.4.86
Release:        0
License:        Apache-2.0
Summary:        Alarm library
Group:          Application Framework/Libraries
Source0:        %{name}-%{version}.tar.gz
Source101:      alarm-server.service
Source102:      60-alarm-manager-rtc.rules
Source103:      alarm-service.conf
Source1001:     %{name}.manifest

BuildRequires:  pkgconfig(appsvc)
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(bundle)
BuildRequires:  pkgconfig(db-util)
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(dbus-glib-1)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(heynoti)
BuildRequires:  pkgconfig(pmapi)
BuildRequires:  pkgconfig(security-server)
BuildRequires:  pkgconfig(tapi)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(libtzplatform-config)

%description
Alarm Server and devel libraries

%package -n alarm-server
Summary:        Alarm server (devel)
Requires(post): /usr/bin/systemctl
Requires(postun): /usr/bin/systemctl
Requires(preun): /usr/bin/systemctl

%description -n alarm-server
Alarm Server

%package -n libalarm
Summary:        Alarm server libraries
Requires:       alarm-server = %{version}-%{release}

%description -n libalarm
Alarm server library

%package -n libalarm-devel
Summary:        Alarm server libraries(devel)
Requires:       libalarm = %{version}-%{release}

%description -n libalarm-devel
Alarm server library (devel)

%prep
%setup -q
cp %{SOURCE1001} .

%build
dbus-binding-tool --mode=glib-server --prefix=alarm_manager ./alarm_mgr.xml > ./include/alarm-skeleton.h
dbus-binding-tool --mode=glib-client --prefix=alarm_manager ./alarm_mgr.xml > ./include/alarm-stub.h
dbus-binding-tool --mode=glib-server --prefix=alarm_client ./alarm-expire.xml > ./include/alarm-expire-skeleton.h
dbus-binding-tool --mode=glib-client --prefix=alarm_client ./alarm-expire.xml > ./include/alarm-expire-stub.h
%reconfigure --disable-static
%__make %{?_smp_mflags}

%install
%make_install

install -d %{buildroot}%{_unitdir}/multi-user.target.wants
install -m0644 %{SOURCE101} %{buildroot}%{_unitdir}
ln -sf ../alarm-server.service %{buildroot}%{_unitdir}/multi-user.target.wants/alarm-server.service

mkdir -p %{buildroot}/%{_sysconfdir}/udev/rules.d
install -m0644  %{SOURCE102} %{buildroot}%{_sysconfdir}/udev/rules.d/

mkdir -p %{buildroot}/%{_sysconfdir}/dbus-1/system.d
install -m0644  %{SOURCE103} %{buildroot}%{_sysconfdir}/dbus-1/system.d/
mkdir -p %{buildroot}%{_datadir}/license
cp LICENSE %{buildroot}%{_datadir}/license/alarm-server
cp LICENSE %{buildroot}%{_datadir}/license/libalarm
cp LICENSE %{buildroot}%{_datadir}/license/libalarm-devel

%preun -n alarm-server
if [ $1 == 0 ]; then
    systemctl stop alarm-server.service
fi

%post -n alarm-server

vconftool set -t int db/system/timechange 0
vconftool set -t double db/system/timechange_external 0
vconftool set -t int memory/system/timechanged 0 -i -g 5000

systemctl daemon-reload
if [ $1 == 1 ]; then
    systemctl restart alarm-server.service
fi

%postun -n alarm-server
systemctl daemon-reload
if [ "$1" == 1 ]; then
    systemctl restart net-config.service
fi

%files -n alarm-server
%manifest %{name}.manifest
%license LICENSE
%attr(0755,root,root) %{_bindir}/alarm-server
%{_unitdir}/multi-user.target.wants/alarm-server.service
%{_unitdir}/alarm-server.service
%{_sysconfdir}/dbus-1/system.d/alarm-service.conf
%ifarch %{arm}
 %exclude %{_sysconfdir}/udev/rules.d/60-alarm-manager-rtc.rules
%else
 %{_sysconfdir}/udev/rules.d/60-alarm-manager-rtc.rules
%endif
%{_datadir}/license/alarm-server

%post -n libalarm -p /sbin/ldconfig

%postun -n libalarm -p /sbin/ldconfig

%files -n libalarm
%manifest %{name}.manifest
%license LICENSE
%attr(0644,root,root) %{_libdir}/libalarm.so.0.0.0
%{_libdir}/libalarm.so.0
%{_datadir}/license/libalarm

%files -n libalarm-devel
%manifest %{name}.manifest
%{_includedir}/*.h
%{_libdir}/pkgconfig/*.pc
%{_libdir}/libalarm.so
%{_datadir}/license/libalarm-devel

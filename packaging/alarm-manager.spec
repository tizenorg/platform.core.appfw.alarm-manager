Name:       alarm-manager
Summary:    Alarm library
Version:    0.4.57
Release:    3
Group:      System/Libraries
License:    Apache License, Version 2.0
Source0:    %{name}-%{version}.tar.gz
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

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

# HACK_removed_dbus_glib_alarm_manager_object_info.diff
#%patch0 -p1

%build

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


%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%post -n alarm-server

heynotitool set setting_time_changed -a
vconftool set -t int db/system/timechange 0 -i

chmod 755 /usr/bin/alarm-server
chmod 755 /etc/init.d/alarm-server_run

mkdir -p /etc/rc.d/rc3.d
mkdir -p /etc/rc.d/rc5.d
ln -s /etc/init.d/alarm-server_run /etc/rc.d/rc3.d/S80alarm-server
ln -s /etc/init.d/alarm-server_run /etc/rc.d/rc5.d/S80alarm-server

%post -n libalarm
chmod 644 /usr/lib/libalarm.so.0.0.0


%files -n alarm-server
%manifest alarm-server.manifest
%{_bindir}/*
/etc/init.d/alarm-server_run

%files -n libalarm
%{_libdir}/*.so.*


%files -n libalarm-devel
%{_includedir}/*.h
%{_libdir}/pkgconfig/*.pc
%{_libdir}/*.so


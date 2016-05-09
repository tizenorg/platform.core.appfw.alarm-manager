Name:       alarm-manager
Summary:    Alarm library
Version:    0.4.178
Release:    1
Group:      System/Libraries
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1:    alarm-server.service
Source2:    99-rtc.rules
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

BuildRequires: cmake
BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(dlog)
BuildRequires: pkgconfig(aul)
BuildRequires: pkgconfig(bundle)
BuildRequires: pkgconfig(sqlite3)
BuildRequires: pkgconfig(db-util)
BuildRequires: pkgconfig(vconf)
BuildRequires: pkgconfig(appsvc)
BuildRequires: pkgconfig(pkgmgr-info)
BuildRequires: pkgconfig(gio-2.0)
BuildRequires: pkgconfig(gio-unix-2.0)
BuildRequires: pkgconfig(capi-system-device)
BuildRequires: pkgconfig(libtzplatform-config)
BuildRequires: pkgconfig(libsystemd-login)
BuildRequires: pkgconfig(eventsystem)
BuildRequires: python-xml

%description
Alarm Server and devel libraries


%package -n alarm-server
Summary:    Alarm server
Group:      Development/Libraries


%description -n alarm-server
Alarm Server, manages alarms


%package -n libalarm
Summary:    Alarm server libraries
Group:      Development/Libraries
Requires:   alarm-server = %{version}-%{release}


%description -n libalarm
Alarm server libraries for client


%package -n libalarm-devel
Summary:    Alarm server libraries (devel)
Group:      Development/Libraries
Requires:   libalarm = %{version}-%{release}


%description -n libalarm-devel
Alarm server libraries development package (devel)


%prep
%setup -q


%build
MAJORVER=`echo %{version} | awk 'BEGIN {FS="."}{print $1}'`

export CFLAGS="$CFLAGS -DTIZEN_DEBUG_ENABLE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_DEBUG_ENABLE"
export FFLAGS="$FFLAGS -DTIZEN_DEBUG_ENABLE"
%define appfw_feature_alarm_manager_module_log 1
%if 0%{?appfw_feature_alarm_manager_module_log}
	_APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG=ON
%endif

%cmake . -DOBS=1 -DFULLVER=%{version} -DMAJORVER=${MAJORVER} -D_APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG:BOOL=${_APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG}

make %{?jobs:-j%jobs}


%install
rm -rf %{buildroot}
%make_install

mkdir -p %{buildroot}%{_unitdir}/multi-user.target.wants
install -m 0644 %SOURCE1 %{buildroot}%{_unitdir}/alarm-server.service
ln -s ../alarm-server.service %{buildroot}%{_unitdir}/multi-user.target.wants/alarm-server.service
mkdir -p %{buildroot}%{_libdir}/udev/rules.d
install -m 0644 %SOURCE2 %{buildroot}%{_libdir}/udev/rules.d

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%post -n alarm-server

%post -n libalarm
/sbin/ldconfig

%postun -n libalarm
/sbin/ldconfig

%files -n alarm-server
%manifest alarm-server.manifest
%{_bindir}/*
%attr(0755,root,root) %{_bindir}/alarm-server
%attr(0644,root,root) %{_unitdir}/alarm-server.service
%{_unitdir}/multi-user.target.wants/alarm-server.service
%attr(0644,root,root) %{_datadir}/dbus-1/system-services/org.tizen.alarm.manager.service
%license LICENSE
%config %{_sysconfdir}/dbus-1/system.d/alarm-service.conf
%{_libdir}/udev/rules.d/99-rtc.rules
%if 0%{?appfw_feature_alarm_manager_module_log}
%attr(0755,root,root) %{_sysconfdir}/dump.d/module.d/alarmmgr_log_dump.sh
%endif

%files -n libalarm
%manifest alarm-lib.manifest
%attr(0644,root,root) %{_libdir}/libalarm.so.*
%{_libdir}/*.so.*
%license LICENSE

%files -n libalarm-devel
%{_includedir}/*.h
%{_libdir}/pkgconfig/*.pc
%{_libdir}/*.so

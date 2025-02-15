/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 *
 * Strawberry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Strawberry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <QtGlobal>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QNetworkProxy>
#include <QSettings>

#include "core/logging.h"
#include "networkproxyfactory.h"

NetworkProxyFactory *NetworkProxyFactory::sInstance = nullptr;
const char *NetworkProxyFactory::kSettingsGroup = "NetworkProxy";

NetworkProxyFactory::NetworkProxyFactory()
    : mode_(Mode_System),
      type_(QNetworkProxy::HttpProxy),
      port_(8080),
      use_authentication_(false) {

#ifdef Q_OS_LINUX
  // Linux uses environment variables to pass proxy configuration information, which systemProxyForQuery doesn't support for some reason.

  QStringList urls;
  urls << QString::fromLocal8Bit(qgetenv("HTTP_PROXY"));
  urls << QString::fromLocal8Bit(qgetenv("http_proxy"));
  urls << QString::fromLocal8Bit(qgetenv("ALL_PROXY"));
  urls << QString::fromLocal8Bit(qgetenv("all_proxy"));

  qLog(Debug) << "Detected system proxy URLs:" << urls;

  for (const QString &url_str : urls) {

    if (url_str.isEmpty()) continue;
    env_url_ = QUrl(url_str);
    break;

  }
#endif

  ReloadSettings();

}

NetworkProxyFactory *NetworkProxyFactory::Instance() {

  if (!sInstance) {
    sInstance = new NetworkProxyFactory;
  }

  return sInstance;

}

void NetworkProxyFactory::ReloadSettings() {

  QMutexLocker l(&mutex_);

  QSettings s;
  s.beginGroup(kSettingsGroup);

  mode_ = Mode(s.value("mode", Mode_System).toInt());
  type_ = QNetworkProxy::ProxyType(s.value("type", QNetworkProxy::HttpProxy).toInt());
  hostname_ = s.value("hostname").toString();
  port_ = s.value("port", 8080).toInt();
  use_authentication_ = s.value("use_authentication", false).toBool();
  username_ = s.value("username").toString();
  password_ = s.value("password").toString();

  s.endGroup();

}

QList<QNetworkProxy> NetworkProxyFactory::queryProxy(const QNetworkProxyQuery &query) {

  QMutexLocker l(&mutex_);

  QNetworkProxy ret;

  switch (mode_) {
    case Mode_System:
#ifdef Q_OS_LINUX
      Q_UNUSED(query);

      if (env_url_.isEmpty()) {
        ret.setType(QNetworkProxy::NoProxy);
      }
      else {
        ret.setHostName(env_url_.host());
        ret.setPort(env_url_.port());
        ret.setUser(env_url_.userName());
        ret.setPassword(env_url_.password());
        if (env_url_.scheme().startsWith("http")) {
          ret.setType(QNetworkProxy::HttpProxy);
        }
        else {
          ret.setType(QNetworkProxy::Socks5Proxy);
        }
        qLog(Debug) << "Using proxy URL:" << env_url_;
      }
      break;
#else
      return systemProxyForQuery(query);
#endif

    case Mode_Direct:
      ret.setType(QNetworkProxy::NoProxy);
      break;

    case Mode_Manual:
      ret.setType(type_);
      ret.setHostName(hostname_);
      ret.setPort(port_);
      if (use_authentication_) {
        ret.setUser(username_);
        ret.setPassword(password_);
      }
      break;
  }

  return QList<QNetworkProxy>() << ret;

}

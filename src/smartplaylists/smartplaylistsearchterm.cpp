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

#include <QObject>
#include <QDataStream>
#include <QVariant>
#include <QString>
#include <QUrl>

#include "smartplaylistsearchterm.h"
#include "playlist/playlist.h"

SmartPlaylistSearchTerm::SmartPlaylistSearchTerm() : field_(Field_Title), operator_(Op_Equals), date_(Date_Hour) {}

SmartPlaylistSearchTerm::SmartPlaylistSearchTerm(Field field, Operator op, const QVariant &value)
    : field_(field), operator_(op), value_(value), date_(Date_Hour) {}

QString SmartPlaylistSearchTerm::ToSql() const {

  QString col = FieldColumnName(field_);
  QString date = DateName(date_, true);
  QString value = value_.toString();
  value.replace('\'', "''");

  if (field_ == Field_Filetype) {
    Song::FileType filetype = Song::FiletypeByExtension(value);
    if (filetype == Song::FileType_Unknown) {
      filetype = Song::FiletypeByDescription(value);
    }
    value = QString::number(static_cast<int>(filetype));
  }

  QString second_value;

  bool special_date_query = (operator_ == SmartPlaylistSearchTerm::Op_NumericDate ||
                             operator_ == SmartPlaylistSearchTerm::Op_NumericDateNot ||
                             operator_ == SmartPlaylistSearchTerm::Op_RelativeDate);

  // Floating point problems...
  // Theoretically 0.0 == 0 stars, 0.1 == 0.5 star, 0.2 == 1 star etc.
  // but in reality we need to consider anything from [0.05, 0.15) range to be 0.5 star etc.
  // To make this simple, I transform the ranges to integeres and then operate on ints: [0.0, 0.05) -> 0, [0.05, 0.15) -> 1 etc.
  if (TypeOf(field_) == Type_Date) {
    if (!special_date_query) {
      // We have the exact date
      // The calendar widget specifies no time so ditch the possible time part
      // from integers representing the dates.
      col = "DATE(" + col + ", 'unixepoch', 'localtime')";
      value = "DATE(" + value + ", 'unixepoch', 'localtime')";
    }
    else {
      // We have a numeric date, consider also the time for more precision
      col = "DATETIME(" + col + ", 'unixepoch', 'localtime')";
      second_value = second_value_.toString();
      second_value.replace('\'', "''");
      if (date == "weeks") {
        // Sqlite doesn't know weeks, transform them to days
        date = "days";
        value = QString::number(value_.toInt() * 7);
        second_value = QString::number(second_value_.toInt() * 7);
      }
    }
  }
  else if (TypeOf(field_) == Type_Time) {
    // Convert seconds to nanoseconds
    value = "CAST (" + value + " *1000000000 AS INTEGER)";
  }

  // File paths need some extra processing since they are stored as encoded urls in the database.
  if (field_ == Field_Filepath) {
    if (operator_ == Op_StartsWith || operator_ == Op_Equals) {
      value = QUrl::fromLocalFile(value).toEncoded();
    }
    else {
      value = QUrl(value).toEncoded();
    }
  }
  else if (TypeOf(field_) == Type_Rating) {
    col = "CAST ((" + col + " + 0.05) * 10 AS INTEGER)";
    value = "CAST ((" + value + " + 0.05) * 10 AS INTEGER)";
  }

  switch (operator_) {
    case Op_Contains:
      return col + " LIKE '%" + value + "%'";
    case Op_NotContains:
      return col + " NOT LIKE '%" + value + "%'";
    case Op_StartsWith:
      return col + " LIKE '" + value + "%'";
    case Op_EndsWith:
      return col + " LIKE '%" + value + "'";
    case Op_Equals:
      if (TypeOf(field_) == Type_Text) {
        return col + " LIKE '" + value + "'";
      }
      else if (TypeOf(field_) == Type_Date || TypeOf(field_) == Type_Time || TypeOf(field_) == Type_Rating) {
        return col + " = " + value;
      }
      else {
        return col + " = '" + value + "'";
      }
    case Op_GreaterThan:
      if (TypeOf(field_) == Type_Date || TypeOf(field_) == Type_Time || TypeOf(field_) == Type_Rating) {
        return col + " > " + value;
      }
      else {
        return col + " > '" + value + "'";
      }
    case Op_LessThan:
      if (TypeOf(field_) == Type_Date || TypeOf(field_) == Type_Time || TypeOf(field_) == Type_Rating) {
        return col + " < " + value;
      }
      else {
        return col + " < '" + value + "'";
      }
    case Op_NumericDate:
      return col + " > " + "DATETIME('now', '-" + value + " " + date + "', 'localtime')";
    case Op_NumericDateNot:
      return col + " < " + "DATETIME('now', '-" + value + " " + date + "', 'localtime')";
    case Op_RelativeDate:
      // Consider the time range before the first date but after the second one
      return "(" + col + " < " + "DATETIME('now', '-" + value + " " + date + "', 'localtime') AND " + col + " > " + "DATETIME('now', '-" + second_value + " " + date + "', 'localtime'))";
    case Op_NotEquals:
      if (TypeOf(field_) == Type_Text) {
        return col + " <> '" + value + "'";
      }
      else {
        return col + " <> " + value;
      }
    case Op_Empty:
      return col + " = ''";
    case Op_NotEmpty:
      return col + " <> ''";
  }

  return QString();
}

bool SmartPlaylistSearchTerm::is_valid() const {

  // We can accept also a zero value in these cases
  if (operator_ == SmartPlaylistSearchTerm::Op_NumericDate) {
    return value_.toInt() >= 0;
  }
  else if (operator_ == SmartPlaylistSearchTerm::Op_RelativeDate) {
    return (value_.toInt() >= 0 && value_.toInt() < second_value_.toInt());
  }

  switch (TypeOf(field_)) {
    case Type_Text:
      if (operator_ == SmartPlaylistSearchTerm::Op_Empty || operator_ == SmartPlaylistSearchTerm::Op_NotEmpty) {
        return true;
      }
      // Empty fields should be possible.
      // All values for Type_Text should be valid.
      return !value_.toString().isEmpty();
    case Type_Date:
      return value_.toInt() != 0;
    case Type_Number:
      return value_.toInt() >= 0;
    case Type_Time:
      return true;
    case Type_Rating:
      return value_.toFloat() >= 0.0;
    case Type_Invalid:
      return false;
  }
  return false;

}

bool SmartPlaylistSearchTerm::operator==(const SmartPlaylistSearchTerm &other) const {
  return field_ == other.field_ && operator_ == other.operator_ &&
         value_ == other.value_ && date_ == other.date_ &&
         second_value_ == other.second_value_;
}

SmartPlaylistSearchTerm::Type SmartPlaylistSearchTerm::TypeOf(const Field field) {

  switch (field) {
    case Field_Length:
      return Type_Time;

    case Field_Track:
    case Field_Disc:
    case Field_Year:
    case Field_OriginalYear:
    case Field_Filesize:
    case Field_PlayCount:
    case Field_SkipCount:
    case Field_Samplerate:
    case Field_Bitdepth:
    case Field_Bitrate:
      return Type_Number;

    case Field_LastPlayed:
    case Field_DateCreated:
    case Field_DateModified:
      return Type_Date;

    case Field_Rating:
      return Type_Rating;

    default:
      return Type_Text;
  }

}

OperatorList SmartPlaylistSearchTerm::OperatorsForType(const Type type) {

  switch (type) {
    case Type_Text:
      return OperatorList() << Op_Contains << Op_NotContains << Op_Equals
                            << Op_NotEquals << Op_Empty << Op_NotEmpty
                            << Op_StartsWith << Op_EndsWith;
    case Type_Date:
      return OperatorList() << Op_Equals << Op_NotEquals << Op_GreaterThan
                            << Op_LessThan << Op_NumericDate
                            << Op_NumericDateNot << Op_RelativeDate;
    default:
      return OperatorList() << Op_Equals << Op_NotEquals << Op_GreaterThan
                            << Op_LessThan;
  }

}

QString SmartPlaylistSearchTerm::OperatorText(const Type type, const Operator op) {

  if (type == Type_Date) {
    switch (op) {
      case Op_GreaterThan:
        return QObject::tr("after");
      case Op_LessThan:
        return QObject::tr("before");
      case Op_Equals:
        return QObject::tr("on");
      case Op_NotEquals:
        return QObject::tr("not on");
      case Op_NumericDate:
        return QObject::tr("in the last");
      case Op_NumericDateNot:
        return QObject::tr("not in the last");
      case Op_RelativeDate:
        return QObject::tr("between");
      default:
        return QString();
    }
  }

  switch (op) {
    case Op_Contains:
      return QObject::tr("contains");
    case Op_NotContains:
      return QObject::tr("does not contain");
    case Op_StartsWith:
      return QObject::tr("starts with");
    case Op_EndsWith:
      return QObject::tr("ends with");
    case Op_GreaterThan:
      return QObject::tr("greater than");
    case Op_LessThan:
      return QObject::tr("less than");
    case Op_Equals:
      return QObject::tr("equals");
    case Op_NotEquals:
      return QObject::tr("not equals");
    case Op_Empty:
      return QObject::tr("empty");
    case Op_NotEmpty:
      return QObject::tr("not empty");
    default:
      return QString();
  }

  return QString();

}

QString SmartPlaylistSearchTerm::FieldColumnName(const Field field) {

  switch (field) {
    case Field_AlbumArtist:
      return "albumartist";
    case Field_Artist:
      return "artist";
    case Field_Album:
      return "album";
    case Field_Title:
      return "title";
    case Field_Track:
      return "track";
    case Field_Disc:
      return "disc";
    case Field_Year:
      return "year";
    case Field_OriginalYear:
      return "originalyear";
    case Field_Genre:
      return "genre";
    case Field_Composer:
      return "composer";
    case Field_Performer:
      return "performer";
    case Field_Grouping:
      return "grouping";
    case Field_Comment:
      return "comment";
    case Field_Length:
      return "length";
    case Field_Filepath:
      return "url";
    case Field_Filetype:
      return "filetype";
    case Field_Filesize:
      return "filesize";
    case Field_DateCreated:
      return "ctime";
    case Field_DateModified:
      return "mtime";
    case Field_PlayCount:
      return "playcount";
    case Field_SkipCount:
      return "skipcount";
    case Field_LastPlayed:
      return "lastplayed";
    case Field_Rating:
      return "rating";
    case Field_Samplerate:
      return "samplerate";
    case Field_Bitdepth:
      return "bitdepth";
    case Field_Bitrate:
      return "bitrate";
    case FieldCount:
      Q_ASSERT(0);
  }
  return QString();

}

QString SmartPlaylistSearchTerm::FieldName(const Field field) {

  switch (field) {
    case Field_AlbumArtist:
      return Playlist::column_name(Playlist::Column_AlbumArtist);
    case Field_Artist:
      return Playlist::column_name(Playlist::Column_Artist);
    case Field_Album:
      return Playlist::column_name(Playlist::Column_Album);
    case Field_Title:
      return Playlist::column_name(Playlist::Column_Title);
    case Field_Track:
      return Playlist::column_name(Playlist::Column_Track);
    case Field_Disc:
      return Playlist::column_name(Playlist::Column_Disc);
    case Field_Year:
      return Playlist::column_name(Playlist::Column_Year);
    case Field_OriginalYear:
      return Playlist::column_name(Playlist::Column_OriginalYear);
    case Field_Genre:
      return Playlist::column_name(Playlist::Column_Genre);
    case Field_Composer:
      return Playlist::column_name(Playlist::Column_Composer);
    case Field_Performer:
      return Playlist::column_name(Playlist::Column_Performer);
    case Field_Grouping:
      return Playlist::column_name(Playlist::Column_Grouping);
    case Field_Comment:
      return QObject::tr("Comment");
    case Field_Length:
      return Playlist::column_name(Playlist::Column_Length);
    case Field_Filepath:
      return Playlist::column_name(Playlist::Column_Filename);
    case Field_Filetype:
      return Playlist::column_name(Playlist::Column_Filetype);
    case Field_Filesize:
      return Playlist::column_name(Playlist::Column_Filesize);
    case Field_DateCreated:
      return Playlist::column_name(Playlist::Column_DateCreated);
    case Field_DateModified:
      return Playlist::column_name(Playlist::Column_DateModified);
    case Field_PlayCount:
      return Playlist::column_name(Playlist::Column_PlayCount);
    case Field_SkipCount:
      return Playlist::column_name(Playlist::Column_SkipCount);
    case Field_LastPlayed:
      return Playlist::column_name(Playlist::Column_LastPlayed);
    case Field_Rating:
      return Playlist::column_name(Playlist::Column_Rating);
    case Field_Samplerate:
      return Playlist::column_name(Playlist::Column_Samplerate);
    case Field_Bitdepth:
      return Playlist::column_name(Playlist::Column_Bitdepth);
    case Field_Bitrate:
      return Playlist::column_name(Playlist::Column_Bitrate);
    case FieldCount:
      Q_ASSERT(0);
  }
  return QString();

}

QString SmartPlaylistSearchTerm::FieldSortOrderText(const Type type, const bool ascending) {

  switch (type) {
    case Type_Text:
      return ascending ? QObject::tr("A-Z") : QObject::tr("Z-A");
    case Type_Date:
      return ascending ? QObject::tr("oldest first") : QObject::tr("newest first");
    case Type_Time:
      return ascending ? QObject::tr("shortest first") : QObject::tr("longest first");
    case Type_Number:
    case Type_Rating:
      return ascending ? QObject::tr("smallest first") : QObject::tr("biggest first");
    case Type_Invalid:
      return QString();
  }
  return QString();

}

QString SmartPlaylistSearchTerm::DateName(const DateType date, const bool forQuery) {

  // If forQuery is true, untranslated keywords are returned
  switch (date) {
    case Date_Hour:
      return (forQuery ? "hours" : QObject::tr("Hours"));
    case Date_Day:
      return (forQuery ? "days" : QObject::tr("Days"));
    case Date_Week:
      return (forQuery ? "weeks" : QObject::tr("Weeks"));
    case Date_Month:
      return (forQuery ? "months" : QObject::tr("Months"));
    case Date_Year:
      return (forQuery ? "years" : QObject::tr("Years"));
  }
  return QString();

}

QDataStream &operator<<(QDataStream &s, const SmartPlaylistSearchTerm &term) {

  s << static_cast<quint8>(term.field_);
  s << static_cast<quint8>(term.operator_);
  s << term.value_;
  s << term.second_value_;
  s << static_cast<quint8>(term.date_);
  return s;

}

QDataStream &operator>>(QDataStream &s, SmartPlaylistSearchTerm &term) {

  quint8 field = 0, op = 0, date = 0;
  s >> field >> op >> term.value_ >> term.second_value_ >> date;
  term.field_ = static_cast<SmartPlaylistSearchTerm::Field>(field);
  term.operator_ = static_cast<SmartPlaylistSearchTerm::Operator>(op);
  term.date_ = static_cast<SmartPlaylistSearchTerm::DateType>(date);
  return s;

}

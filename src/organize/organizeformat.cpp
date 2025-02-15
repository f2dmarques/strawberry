/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QApplication>
#include <QChar>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>
#include <QColor>
#include <QPalette>
#include <QValidator>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextFormat>

#include "core/arraysize.h"
#include "core/song.h"
#include "utilities/transliterate.h"
#include "utilities/timeconstants.h"

#include "organizeformat.h"

const QRegularExpression OrganizeFormat::kProblematicCharacters("[:?*\"<>|]");
// From http://en.wikipedia.org/wiki/8.3_filename#Directory_table
const QRegularExpression OrganizeFormat::kInvalidFatCharacters("[^a-zA-Z0-9!#\\$%&'()\\-@\\^_`{}~/. ]", QRegularExpression::CaseInsensitiveOption);
const QRegularExpression OrganizeFormat::kInvalidDirCharacters("[/\\\\]");
constexpr char OrganizeFormat::kInvalidPrefixCharacters[] = ".";
constexpr int OrganizeFormat::kInvalidPrefixCharactersCount = arraysize(OrganizeFormat::kInvalidPrefixCharacters) - 1;

constexpr char OrganizeFormat::kBlockPattern[] = "\\{([^{}]+)\\}";
constexpr char OrganizeFormat::kTagPattern[] = "\\%([a-zA-Z]*)";

const QStringList OrganizeFormat::kKnownTags = QStringList() << "title"
                                                             << "album"
                                                             << "artist"
                                                             << "artistinitial"
                                                             << "albumartist"
                                                             << "composer"
                                                             << "track"
                                                             << "disc"
                                                             << "year"
                                                             << "originalyear"
                                                             << "genre"
                                                             << "comment"
                                                             << "length"
                                                             << "bitrate"
                                                             << "samplerate"
                                                             << "bitdepth"
                                                             << "extension"
                                                             << "performer"
                                                             << "grouping"
                                                             << "lyrics";

const QStringList OrganizeFormat::kUniqueTags = QStringList() << "title"
                                                              << "track";

const QRgb OrganizeFormat::SyntaxHighlighter::kValidTagColorLight = qRgb(64, 64, 255);
const QRgb OrganizeFormat::SyntaxHighlighter::kInvalidTagColorLight = qRgb(255, 64, 64);
const QRgb OrganizeFormat::SyntaxHighlighter::kBlockColorLight = qRgb(230, 230, 230);

const QRgb OrganizeFormat::SyntaxHighlighter::kValidTagColorDark = qRgb(128, 128, 255);
const QRgb OrganizeFormat::SyntaxHighlighter::kInvalidTagColorDark = qRgb(255, 128, 128);
const QRgb OrganizeFormat::SyntaxHighlighter::kBlockColorDark = qRgb(64, 64, 64);

OrganizeFormat::OrganizeFormat(const QString &format)
    : format_(format),
      remove_problematic_(false),
      remove_non_fat_(false),
      remove_non_ascii_(false),
      allow_ascii_ext_(false),
      replace_spaces_(true) {}

void OrganizeFormat::set_format(const QString &v) {
  format_ = v;
  format_.replace('\\', '/');
}

bool OrganizeFormat::IsValid() const {

  int pos = 0;
  QString format_copy(format_);

  Validator v;
  return v.validate(format_copy, pos) == QValidator::Acceptable;

}

OrganizeFormat::GetFilenameForSongResult OrganizeFormat::GetFilenameForSong(const Song &song, QString extension) const {

  bool unique_filename = false;
  QString filepath = ParseBlock(format_, song, &unique_filename);

  if (filepath.isEmpty()) {
    filepath = song.basefilename();
  }

  {
    QFileInfo fileinfo(filepath);
    if (fileinfo.completeBaseName().isEmpty()) {
      // Avoid having empty filenames, or filenames with extension only: in this case, keep the original filename.
      // We remove the extension from "filename" if it exists, as song.basefilename() also contains the extension.
      QString path = fileinfo.path();
      filepath.clear();
      if (!path.isEmpty()) {
        filepath.append(path);
        if (path.right(1) != '/') {
          filepath.append('/');
        }
      }
      filepath.append(song.basefilename());
    }
  }

  if (filepath.isEmpty() || (filepath.contains('/') && (filepath.section('/', 0, -2).isEmpty() || filepath.section('/', 0, -2).isEmpty()))) {
    return GetFilenameForSongResult();
  }

  if (remove_problematic_) filepath = filepath.remove(kProblematicCharacters);
  if (remove_non_fat_ || (remove_non_ascii_ && !allow_ascii_ext_)) filepath = Utilities::Transliterate(filepath);
  if (remove_non_fat_) filepath = filepath.remove(kInvalidFatCharacters);

  if (remove_non_ascii_) {
    int ascii = 128;
    if (allow_ascii_ext_) ascii = 255;
    QString stripped;
    for (int i = 0; i < filepath.length(); ++i) {
      const QChar c = filepath[i];
      if (c.unicode() < ascii) {
        stripped.append(c);
      }
      else {
        const QString decomposition = c.decomposition();
        if (!decomposition.isEmpty() && decomposition[0].unicode() < ascii) {
          stripped.append(decomposition[0]);
        }
      }
    }
    filepath = stripped;
  }

  // Remove repeated whitespaces in the filepath.
  filepath = filepath.simplified();

  // Fixup extension
  QFileInfo info(filepath);
  filepath.clear();
  if (extension.isEmpty()) {
    if (info.completeSuffix().isEmpty()) {
      extension = QFileInfo(song.url().toLocalFile()).completeSuffix();
    }
    else {
      extension = info.completeSuffix();
    }
  }
  if (!info.path().isEmpty() && info.path() != ".") {
    filepath.append(info.path());
    filepath.append("/");
  }
  filepath.append(info.completeBaseName());

  // Fix any parts of the path that start with dots.
  QStringList parts_old = filepath.split("/");
  QStringList parts_new;
  for (int i = 0; i < parts_old.count(); ++i) {
    QString part = parts_old[i];
    for (int j = 0; j < kInvalidPrefixCharactersCount; ++j) {
      if (part.startsWith(kInvalidPrefixCharacters[j])) {
        part = part.remove(0, 1);
        break;
      }
    }
    part = part.trimmed();
    parts_new.append(part);
  }
  filepath = parts_new.join("/");

  if (replace_spaces_) filepath.replace(QRegularExpression("\\s"), "_");

  if (!extension.isEmpty()) {
    filepath.append(QString(".%1").arg(extension));
  }

  return GetFilenameForSongResult(filepath, unique_filename);

}

QString OrganizeFormat::ParseBlock(QString block, const Song &song, bool *have_tagdata, bool *any_empty) const {

  // Find any blocks first
  qint64 pos = 0;
  const QRegularExpression block_regexp(kBlockPattern);
  QRegularExpressionMatch re_match;
  for (re_match = block_regexp.match(block, pos); re_match.hasMatch(); re_match = block_regexp.match(block, pos)) {
    pos = re_match.capturedStart();
    // Recursively parse the block
    bool empty = false;
    QString value = ParseBlock(re_match.captured(1), song, have_tagdata, &empty);
    if (empty) value = "";

    // Replace the block's value
    block.replace(pos, re_match.capturedLength(), value);
    pos += value.length();
  }

  // Now look for tags
  bool empty = false;
  pos = 0;
  const QRegularExpression tag_regexp(kTagPattern);
  for (re_match = tag_regexp.match(block, pos); re_match.hasMatch(); re_match = tag_regexp.match(block, pos)) {
    pos = re_match.capturedStart();
    const QString tag = re_match.captured(1);
    const QString value = TagValue(tag, song);
    if (value.isEmpty()) {
      empty = true;
    }
    else if (have_tagdata && kUniqueTags.contains(tag)) {
      *have_tagdata = true;
    }

    block.replace(pos, re_match.capturedLength(), value);
    pos += value.length();
  }

  if (any_empty) {
    *any_empty = empty;
  }

  return block;

}

QString OrganizeFormat::TagValue(const QString &tag, const Song &song) const {

  QString value;

  if (tag == "title") {
    value = song.title();
  }
  else if (tag == "album") {
    value = song.album();
  }
  else if (tag == "artist") {
    value = song.artist();
  }
  else if (tag == "composer") {
    value = song.composer();
  }
  else if (tag == "performer") {
    value = song.performer();
  }
  else if (tag == "grouping") {
    value = song.grouping();
  }
  else if (tag == "lyrics") {
    value = song.lyrics();
  }
  else if (tag == "genre") {
    value = song.genre();
  }
  else if (tag == "comment") {
    value = song.comment();
  }
  else if (tag == "year") {
    value = QString::number(song.year());
  }
  else if (tag == "originalyear") {
    value = QString::number(song.effective_originalyear());
  }
  else if (tag == "track") {
    value = QString::number(song.track());
  }
  else if (tag == "disc") {
    value = QString::number(song.disc());
  }
  else if (tag == "length") {
    value = QString::number(song.length_nanosec() / kNsecPerSec);
  }
  else if (tag == "bitrate") {
    value = QString::number(song.bitrate());
  }
  else if (tag == "samplerate") {
    value = QString::number(song.samplerate());
  }
  else if (tag == "bitdepth") {
    value = QString::number(song.bitdepth());
  }
  else if (tag == "extension") {
    value = QFileInfo(song.url().toLocalFile()).suffix();
  }
  else if (tag == "artistinitial") {
    value = song.effective_albumartist().trimmed();
    if (!value.isEmpty()) {
      value.replace(QRegularExpression("^the\\s+", QRegularExpression::CaseInsensitiveOption), "");
      value = value[0].toUpper();
    }
  }
  else if (tag == "albumartist") {
    value = song.is_compilation() ? "Various Artists" : song.effective_albumartist();
  }

  if (value == "0" || value == "-1") value = "";

  // Prepend a 0 to single-digit track numbers
  if (tag == "track" && value.length() == 1) value.prepend('0');

  // Replace characters that really shouldn't be in paths
  value = value.remove(kInvalidDirCharacters);
  if (remove_problematic_) value = value.remove('.');
  value = value.trimmed();

  return value;

}

OrganizeFormat::Validator::Validator(QObject *parent) : QValidator(parent) {}

QValidator::State OrganizeFormat::Validator::validate(QString &input, int&) const {

  // Make sure all the blocks match up
  int block_level = 0;
  for (int i = 0; i < input.length(); ++i) {
    if (input[i] == '{') {
      ++block_level;
    }
    else if (input[i] == '}') {
      --block_level;
    }

    if (block_level < 0 || block_level > 1) return QValidator::Invalid;
  }

  if (block_level != 0) return QValidator::Invalid;

  // Make sure the tags are valid
  const QRegularExpression tag_regexp(kTagPattern);
  QRegularExpressionMatch re_match;
  qint64 pos = 0;
  for (re_match = tag_regexp.match(input, pos); re_match.hasMatch(); re_match = tag_regexp.match(input, pos)) {
    pos = re_match.capturedStart();
    if (!OrganizeFormat::kKnownTags.contains(re_match.captured(1))) {
      return QValidator::Invalid;
    }

    pos += re_match.capturedLength();
  }

  return QValidator::Acceptable;

}

OrganizeFormat::SyntaxHighlighter::SyntaxHighlighter(QObject *parent) : QSyntaxHighlighter(parent) {}

OrganizeFormat::SyntaxHighlighter::SyntaxHighlighter(QTextEdit *parent) : QSyntaxHighlighter(parent) {}

OrganizeFormat::SyntaxHighlighter::SyntaxHighlighter(QTextDocument *parent) : QSyntaxHighlighter(parent) {}

void OrganizeFormat::SyntaxHighlighter::highlightBlock(const QString &text) {

  const bool light = QApplication::palette().color(QPalette::Base).value() > 128;
  const QRgb block_color = light ? kBlockColorLight : kBlockColorDark;
  const QRgb valid_tag_color = light ? kValidTagColorLight : kValidTagColorDark;
  const QRgb invalid_tag_color = light ? kInvalidTagColorLight : kInvalidTagColorDark;

  QTextCharFormat block_format;
  block_format.setBackground(QColor(block_color));

  // Reset formatting
  setFormat(0, static_cast<int>(text.length()), QTextCharFormat());

  // Blocks
  const QRegularExpression block_regexp(kBlockPattern);
  QRegularExpressionMatch re_match;
  qint64 pos = 0;
  for (re_match = block_regexp.match(text, pos); re_match.hasMatch(); re_match = block_regexp.match(text, pos)) {
    pos = re_match.capturedStart();
    setFormat(static_cast<int>(pos), static_cast<int>(re_match.capturedLength()), block_format);
    pos += re_match.capturedLength();
  }

  // Tags
  const QRegularExpression tag_regexp(kTagPattern);
  pos = 0;
  for (re_match = tag_regexp.match(text, pos); re_match.hasMatch(); re_match = tag_regexp.match(text, pos)) {
    pos = re_match.capturedStart();
    QTextCharFormat f = format(static_cast<int>(pos));
    f.setForeground(QColor(OrganizeFormat::kKnownTags.contains(re_match.captured(1)) ? valid_tag_color : invalid_tag_color));

    setFormat(static_cast<int>(pos), static_cast<int>(re_match.capturedLength()), f);
    pos += re_match.capturedLength();
  }

}

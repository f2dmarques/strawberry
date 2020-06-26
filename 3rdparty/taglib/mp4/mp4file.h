/**************************************************************************
    copyright            : (C) 2007 by Lukáš Lalinský
    email                : lalinsky@gmail.com
 **************************************************************************/

/***************************************************************************
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License version   *
 *   2.1 as published by the Free Software Foundation.                     *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful, but   *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA         *
 *   02110-1301  USA                                                       *
 *                                                                         *
 *   Alternatively, this file is available under the Mozilla Public        *
 *   License Version 1.1.  You may obtain a copy of the License at         *
 *   http://www.mozilla.org/MPL/                                           *
 ***************************************************************************/

#ifndef TAGLIB_MP4FILE_H
#define TAGLIB_MP4FILE_H

#include "tag.h"
#include "tfile.h"
#include "taglib_export.h"
#include "mp4properties.h"
#include "mp4tag.h"

namespace Strawberry_TagLib {
namespace TagLib {

//! An implementation of MP4 (AAC, ALAC, ...) metadata
namespace MP4 {

class Atoms;

/*!
 * This implements and provides an interface for MP4 files to the
 * TagLib::Tag and TagLib::AudioProperties interfaces by way of implementing
 * the abstract TagLib::File API as well as providing some additional
 * information specific to MP4 files.
 */
class TAGLIB_EXPORT File : public Strawberry_TagLib::TagLib::File {
 public:
  /*!
   * Constructs an MP4 file from \a file.
   * If \a readProperties is true the file's audio properties will also be read.
   *
   * \note In the current implementation, \a propertiesStyle is ignored.
   */
  explicit File(FileName file, bool readProperties = true, AudioProperties::ReadStyle audioPropertiesStyle = AudioProperties::Average);

  /*!
   * Constructs an MP4 file from \a stream.
   * If \a readProperties is true the file's audio properties will also be read.
   *
   * \note TagLib will *not* take ownership of the stream, the caller is
   * responsible for deleting it after the File object.
   *
   * \note In the current implementation, \a propertiesStyle is ignored.
   */
  explicit File(IOStream *stream, bool readProperties = true, AudioProperties::ReadStyle audioPropertiesStyle = AudioProperties::Average);

  /*!
   * Destroys this instance of the File.
   */
  ~File() override;

  /*!
   * Returns a pointer to the MP4 tag of the file.
   *
   * MP4::Tag implements the tag interface, so this serves as the reimplementation of TagLib::File::tag().
   *
   * \note The Tag <b>is still</b> owned by the MP4::File and should not be deleted by the user.
   * It will be deleted when the file (object) is destroyed.
   */
  Tag *tag() const override;

  /*!
   * Returns the MP4 audio properties for this file.
   */
  AudioProperties *audioProperties() const override;

  /*!
   * Save the file.
   *
   * This returns true if the save was successful.
   */
  bool save() override;

  /*!
   * Returns whether or not the file on disk actually has an MP4 tag, or the file has a Metadata Item List (ilst) atom.
   */
  bool hasMP4Tag() const;

  /*!
   * Returns whether or not the given \a stream can be opened as an ASF file.
   *
   * \note This method is designed to do a quick check.  The result may not necessarily be correct.
   */
  static bool isSupported(IOStream *stream);

 private:
  void read(bool readProperties);

  class FilePrivate;
  FilePrivate *d;
};

}  // namespace MP4
}  // namespace TagLib
}  // namespace Strawberry_TagLib

#endif

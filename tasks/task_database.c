/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2017 - Brad Parker
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <compat/strcasestr.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include <retro_endianness.h>
#include <string/stdstring.h>
#include <lists/dir_list.h>
#include <file/file_path.h>
#include <encodings/crc32.h>
#include <streams/file_stream.h>
#include <streams/chd_stream.h>
#include <streams/interface_stream.h>
#include "tasks_internal.h"

#include "../database_info.h"

#include "../file_path_special.h"
#include "../list_special.h"
#include "../msg_hash.h"
#include "../playlist.h"
#include "../retroarch.h"
#include "../verbosity.h"
#include "../core_info.h"

#ifndef COLLECTION_SIZE
#define COLLECTION_SIZE                99999
#endif

typedef struct database_state_handle
{
   database_info_list_t *info;
   struct string_list *list;
   size_t list_index;
   size_t entry_index;
   uint32_t crc;
   uint32_t archive_crc;
   uint8_t *buf;
   char archive_name[511];
   char serial[4096];
} database_state_handle_t;

typedef struct db_handle
{
   database_state_handle_t state;
   database_info_handle_t *handle;
   unsigned status;
   char playlist_directory[4096];
   char content_database_path[4096];

   bool is_directory;
   char fullpath[4096];
   bool scan_started;
} db_handle_t;

int cue_find_track(const char *cue_path, bool first, size_t *offset, size_t *size,
               char *track_path, size_t max_len);

int gdi_find_track(const char *gdi_path, bool first, char *track_path, size_t max_len);

int detect_system(intfstream_t *fd, const char** system_name);

int detect_ps1_game(intfstream_t *fd, char *game_id);

int detect_psp_game(intfstream_t *fd, char *game_id);

int detect_serial_ascii_game(intfstream_t *fd, char *game_id);

static intfstream_t* open_file(const char *path)
{
   intfstream_info_t info;
   intfstream_t *fd = NULL;

   info.type        = INTFSTREAM_FILE;
   fd               = intfstream_init(&info);

   if (!fd)
      return NULL;

   if (!intfstream_open(fd, path, RFILE_MODE_READ, -1))
   {
      intfstream_close(fd);
      return NULL;
   }

   return fd;
}

static intfstream_t*
open_memory(void *data, size_t size)
{
   intfstream_info_t info;
   intfstream_t *fd = NULL;

   info.type = INTFSTREAM_MEMORY;
   info.memory.buf.data = data;
   info.memory.buf.size = size;
   info.memory.writable = false;

   fd = intfstream_init(&info);
   if (!fd)
   {
      return NULL;
   }

   if (!intfstream_open(fd, NULL, RFILE_MODE_READ, -1))
   {
      intfstream_close(fd);
      return NULL;
   }

   return fd;
}

static intfstream_t*
open_chd_track(const char *path, int32_t track)
{
   intfstream_info_t info;
   intfstream_t *fd = NULL;

   info.type        = INTFSTREAM_CHD;
   info.chd.track   = track;

   fd               = intfstream_init(&info);

   if (!fd)
      return NULL;

   if (!intfstream_open(fd, path, RFILE_MODE_READ, -1))
   {
      intfstream_close(fd);
      return NULL;
   }

   return fd;
}

static void database_info_set_type(
      database_info_handle_t *handle,
      enum database_type type)
{
   if (!handle)
      return;
   handle->type = type;
}

static enum database_type database_info_get_type(
      database_info_handle_t *handle)
{
   if (!handle)
      return DATABASE_TYPE_NONE;
   return handle->type;
}

static const char *database_info_get_current_name(
      database_state_handle_t *handle)
{
   if (!handle || !handle->list)
      return NULL;
   return handle->list->elems[handle->list_index].data;
}

static const char *database_info_get_current_element_name(
      database_info_handle_t *handle)
{
   if (!handle || !handle->list)
      return NULL;
   return handle->list->elems[handle->list_ptr].data;
}

static int task_database_iterate_start(database_info_handle_t *db,
      const char *name)
{
   char msg[511];

   msg[0] = msg[510] = '\0';

   snprintf(msg, sizeof(msg),
         STRING_REP_ULONG "/" STRING_REP_ULONG ": %s %s...\n",
#if defined(_WIN32) || defined(__STDC_VERSION__) && __STDC_VERSION__>=199901L && !defined(VITA) &&!defined(WIIU)
         db->list_ptr,
         db->list->size,
#else
         (unsigned long)db->list_ptr,
         (unsigned long)db->list->size,
#endif
         msg_hash_to_str(MSG_SCANNING),
         name);

   if (!string_is_empty(msg))
      runloop_msg_queue_push(msg, 1, 180, true);

#if 0
   RARCH_LOG("msg: %s\n", msg);
#endif


   db->status = DATABASE_STATUS_ITERATE;

   return 0;
}

static int stream_get_serial(intfstream_t *fd, char *serial)
{
  const char *system_name = NULL;

  /* Check if the system was not auto-detected. */
  if (detect_system(fd, &system_name) < 0)
  {
    /* Attempt to read an ASCII serial, like Wii. */
    if (detect_serial_ascii_game(fd, serial))
    {
      /* ASCII serial (Wii) was detected. */
      RARCH_LOG("%s '%s'\n", msg_hash_to_str(MSG_FOUND_DISK_LABEL), serial);
      return 0;
    }

    /* Any other non-system specific detection methods? */
    return 0;
  }

  if (string_is_equal_fast(system_name, "psp", 3))
  {
    if (detect_psp_game(fd, serial) == 0)
      return 0;
    RARCH_LOG("%s '%s'\n", msg_hash_to_str(MSG_FOUND_DISK_LABEL), serial);
  }
  else if (string_is_equal_fast(system_name, "ps1", 3))
  {
    if (detect_ps1_game(fd, serial) == 0)
      return 0;
    RARCH_LOG("%s '%s'\n", msg_hash_to_str(MSG_FOUND_DISK_LABEL), serial);
  }
  else {
    return 0;
  }

  return 1;
}

static bool file_get_serial(const char *name, size_t offset, size_t size, char *serial)
{
   intfstream_t *fd = open_file(name);
   int rv;
   uint8_t *data = NULL;
   ssize_t file_size = -1;

   if (!fd)
      return 0;

   intfstream_seek(fd, 0, SEEK_END);
   file_size = intfstream_tell(fd);
   intfstream_seek(fd, 0, SEEK_SET);
   if (file_size < 0) {
      intfstream_close(fd);
      return 0;
   }

   if (offset != 0 || size < (size_t) file_size)
   {
      data = malloc(size);
      intfstream_seek(fd, offset, SEEK_SET);
      if (intfstream_read(fd, data, size) != (ssize_t) size)
      {
         intfstream_close(fd);
         free(data);
         return 0;
      }
      intfstream_close(fd);
      fd = open_memory(data, size);
      if (!fd) {
         free(data);
         return 0;
      }
   }

   rv = stream_get_serial(fd, serial);
   intfstream_close(fd);
   free(data);
   return rv;
}

static int cue_get_serial(const char *name, char* serial)
{
   char *track_path                 = (char*)malloc(PATH_MAX_LENGTH 
         * sizeof(char));
   int ret                          = 0;
   size_t offset                    = 0;
   size_t size                      = 0;
   int rv                           = 0;

   track_path[0]                    = '\0';

   rv = cue_find_track(name, true, &offset, &size, track_path, PATH_MAX_LENGTH);

   if (rv < 0)
   {
      RARCH_LOG("%s: %s\n",
            msg_hash_to_str(MSG_COULD_NOT_FIND_VALID_DATA_TRACK),
            strerror(-rv));
      free(track_path);
      return 0;
   }

   RARCH_LOG("%s\n", msg_hash_to_str(MSG_READING_FIRST_DATA_TRACK));

   ret = file_get_serial(track_path, offset, size, serial);
   free(track_path);

   return ret;
}

static int gdi_get_serial(const char *name, char* serial)
{
   char *track_path                 = (char*)malloc(PATH_MAX_LENGTH
         * sizeof(char));
   int ret                          = 0;
   int rv                           = 0;

   track_path[0]                    = '\0';

   rv = gdi_find_track(name, true, track_path, PATH_MAX_LENGTH);

   if (rv < 0)
   {
      RARCH_LOG("%s: %s\n",
            msg_hash_to_str(MSG_COULD_NOT_FIND_VALID_DATA_TRACK),
            strerror(-rv));
      free(track_path);
      return 0;
   }

   RARCH_LOG("%s\n", msg_hash_to_str(MSG_READING_FIRST_DATA_TRACK));

   ret = file_get_serial(track_path, 0, SIZE_MAX, serial);
   free(track_path);

   return ret;
}

static int chd_get_serial(const char *name, char* serial)
{
   intfstream_t *fd = NULL;
   int result;

   fd = open_chd_track(name, CHDSTREAM_TRACK_FIRST_DATA);
   if (!fd)
   {
      return 0;
   }

   result = stream_get_serial(fd, serial);
   intfstream_close(fd);
   return result;
}

static int stream_get_crc(intfstream_t *fd, uint32_t *crc)
{
   size_t read = 0;
   uint32_t acc = 0;
   uint8_t buffer[4096];

   while ((read = intfstream_read(fd, buffer, sizeof(buffer))) > 0)
   {
      acc = encoding_crc32(acc, buffer, read);
   }

   if (read < 0)
   {
      return 0;
   }

   *crc = acc;

   return 1;
}

static bool file_get_crc(const char *name, size_t offset, size_t size, uint32_t *crc)
{
   intfstream_t *fd = open_file(name);
   int rv;
   uint8_t *data = NULL;
   ssize_t file_size = -1;

   if (!fd)
   {
      return 0;
   }

   intfstream_seek(fd, 0, SEEK_END);
   file_size = intfstream_tell(fd);
   intfstream_seek(fd, 0, SEEK_SET);
   if (file_size < 0) {
      intfstream_close(fd);
      return 0;
   }

   if (offset != 0 || size < (size_t) file_size) {
      data = malloc(size);
      intfstream_seek(fd, offset, SEEK_SET);
      if (intfstream_read(fd, data, size) != (ssize_t) size) {
         intfstream_close(fd);
         free(data);
         return 0;
      }
      intfstream_close(fd);
      fd = open_memory(data, size);
      if (!fd) {
         free(data);
         return 0;
      }
   }

   rv = stream_get_crc(fd, crc);
   intfstream_close(fd);
   free(data);
   return rv;
}

static int cue_get_crc(const char *name, uint32_t *crc)
{
   char *track_path = (char *)malloc(PATH_MAX_LENGTH);
   int ret = 0;
   size_t offset = 0;
   size_t size = 0;
   int rv = 0;

   track_path[0] = '\0';

   rv = cue_find_track(name, false, &offset, &size, track_path, PATH_MAX_LENGTH);

   if (rv < 0) {
      RARCH_LOG("%s: %s\n", msg_hash_to_str(MSG_COULD_NOT_FIND_VALID_DATA_TRACK),
                strerror(-rv));
      free(track_path);
      return 0;
   }

   RARCH_LOG("CUE '%s' primary track: %s\n (%Zu, %Zu)", name, track_path, offset, size);

   RARCH_LOG("%s\n", msg_hash_to_str(MSG_READING_FIRST_DATA_TRACK));

   rv = file_get_crc(track_path, offset, size, crc);
   if (rv == 1) {
      RARCH_LOG("CUE '%s' crc: %x\n", name, *crc);
   }
   free(track_path);
   return rv;
}

static int gdi_get_crc(const char *name, uint32_t *crc)
{
   char *track_path = (char *)malloc(PATH_MAX_LENGTH);
   int ret = 0;
   int rv = 0;

   track_path[0] = '\0';

   rv = gdi_find_track(name, false, track_path, PATH_MAX_LENGTH);

   if (rv < 0) {
      RARCH_LOG("%s: %s\n", msg_hash_to_str(MSG_COULD_NOT_FIND_VALID_DATA_TRACK),
                strerror(-rv));
      free(track_path);
      return 0;
   }

   RARCH_LOG("GDI '%s' primary track: %s\n", name, track_path);

   RARCH_LOG("%s\n", msg_hash_to_str(MSG_READING_FIRST_DATA_TRACK));

   rv = file_get_crc(track_path, 0, SIZE_MAX, crc);
   if (rv == 1) {
      RARCH_LOG("GDI '%s' crc: %x\n", name, *crc);
   }
   free(track_path);
   return rv;
}

static bool chd_get_crc(const char *name, uint32_t *crc)
{
   intfstream_t *fd = NULL;
   int rv;
   uint32_t acc = 0;
   uint8_t buffer[4096];
   ssize_t size;

   fd = open_chd_track(name, CHDSTREAM_TRACK_PRIMARY);
   if (!fd)
   {
      return 0;
   }

   rv = stream_get_crc(fd, crc);
   if (rv == 1) {
      RARCH_LOG("CHD '%s' crc: %x\n", name, *crc);
   }
   intfstream_close(fd);
   return rv;
}

static int task_database_iterate_playlist(
      database_state_handle_t *db_state,
      database_info_handle_t *db, const char *name)
{
   switch (msg_hash_to_file_type(msg_hash_calculate(path_get_extension(name))))
   {
      case FILE_TYPE_COMPRESSED:
#ifdef HAVE_COMPRESSION
         database_info_set_type(db, DATABASE_TYPE_CRC_LOOKUP);
         /* first check crc of archive itself */
         return file_get_crc(name, 0, SIZE_MAX, &db_state->archive_crc);
#else
         break;
#endif
      case FILE_TYPE_CUE:
         db_state->serial[0] = '\0';
         if (cue_get_serial(name, db_state->serial))
         {
            database_info_set_type(db, DATABASE_TYPE_SERIAL_LOOKUP);
         }
         else
         {
            database_info_set_type(db, DATABASE_TYPE_CRC_LOOKUP);
            return cue_get_crc(name, &db_state->crc);
         }
         break;
      case FILE_TYPE_GDI:
         db_state->serial[0] = '\0';
         if (gdi_get_serial(name, db_state->serial))
         {
            database_info_set_type(db, DATABASE_TYPE_SERIAL_LOOKUP);
         }
         else
         {
            database_info_set_type(db, DATABASE_TYPE_CRC_LOOKUP);
            return gdi_get_crc(name, &db_state->crc);
         }
         break;
      case FILE_TYPE_ISO:
         db_state->serial[0] = '\0';
         file_get_serial(name, 0, SIZE_MAX, db_state->serial);
         database_info_set_type(db, DATABASE_TYPE_SERIAL_LOOKUP);
         break;
      case FILE_TYPE_CHD:
         db_state->serial[0] = '\0';
         /* There are no serial databases, so don't bother with
            serials at the moment */
         if (0 && chd_get_serial(name, db_state->serial))
         {
            database_info_set_type(db, DATABASE_TYPE_SERIAL_LOOKUP);
         }
         else
         {
            database_info_set_type(db, DATABASE_TYPE_CRC_LOOKUP);
            return chd_get_crc(name, &db_state->crc);
         }
         break;
      case FILE_TYPE_LUTRO:
         database_info_set_type(db, DATABASE_TYPE_ITERATE_LUTRO);
         break;
      default:
         database_info_set_type(db, DATABASE_TYPE_CRC_LOOKUP);
         return file_get_crc(name, 0, SIZE_MAX, &db_state->crc);
   }

   return 1;
}

static int database_info_list_iterate_end_no_match(
      database_state_handle_t *db_state)
{
   /* Reached end of database list,
    * CRC match probably didn't succeed. */
   db_state->list_index  = 0;
   db_state->entry_index = 0;

   if (db_state->crc != 0)
      db_state->crc = 0;

   return 0;
}

static int task_database_iterate_next(database_info_handle_t *db)
{
   db->list_ptr++;

   if (db->list_ptr < db->list->size)
      return 0;
   return -1;
}

static int database_info_list_iterate_new(database_state_handle_t *db_state,
      const char *query)
{
   const char *new_database = database_info_get_current_name(db_state);

#if 0
   RARCH_LOG("Check database [%d/%d] : %s\n", (unsigned)db_state->list_index,
         (unsigned)db_state->list->size, new_database);
#endif
   if (db_state->info)
   {
      database_info_list_free(db_state->info);
      free(db_state->info);
   }
   db_state->info = database_info_list_new(new_database, query);
   return 0;
}

static int database_info_list_iterate_found_match(
      db_handle_t *_db,
      database_state_handle_t *db_state,
      database_info_handle_t *db,
      const char *archive_name
      )
{
   char *db_crc                   = (char*)malloc(128 * sizeof(char));
   char *db_playlist_base_str     = (char*)malloc(128 * sizeof(char));
   char *db_playlist_path         = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   char *entry_path_str           = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   playlist_t   *playlist         = NULL;
   const char         *db_path    =
      database_info_get_current_name(db_state);
   const char         *entry_path =
      database_info_get_current_element_name(db);
   database_info_t *db_info_entry =
      &db_state->info->list[db_state->entry_index];

   db_crc[0] = '\0';
   db_playlist_path[0] = '\0';
   db_playlist_base_str[0] = '\0';
   entry_path_str[0] = '\0';

   fill_short_pathname_representation_noext(db_playlist_base_str,
         db_path, 128 * sizeof(char));

   strlcat(db_playlist_base_str,
         file_path_str(FILE_PATH_LPL_EXTENSION),
         128 * sizeof(char));
   fill_pathname_join(db_playlist_path, _db->playlist_directory,
         db_playlist_base_str, PATH_MAX_LENGTH * sizeof(char));

   playlist = playlist_init(db_playlist_path, COLLECTION_SIZE);

   snprintf(db_crc, 128 * sizeof(char),
         "%08X|crc", db_info_entry->crc32);

   if (entry_path)
      strlcpy(entry_path_str, entry_path, 128 * sizeof(char));

   if (!string_is_empty(archive_name))
      fill_pathname_join_delim(entry_path_str,
            entry_path_str, archive_name,
            '#', 128 * sizeof(char));

#if 0
   RARCH_LOG("Found match in database !\n");

   RARCH_LOG("Path: %s\n", db_path);
   RARCH_LOG("CRC : %s\n", db_crc);
   RARCH_LOG("Playlist Path: %s\n", db_playlist_path);
   RARCH_LOG("Entry Path: %s\n", entry_path);
   RARCH_LOG("Playlist not NULL: %d\n", playlist != NULL);
   RARCH_LOG("ZIP entry: %s\n", archive_name);
   RARCH_LOG("entry path str: %s\n", entry_path_str);
#endif

   if(!playlist_entry_exists(playlist, entry_path_str, db_crc))
   {
      playlist_push(playlist, entry_path_str,
            db_info_entry->name,
            file_path_str(FILE_PATH_DETECT),
            file_path_str(FILE_PATH_DETECT),
            db_crc, db_playlist_base_str);
   }

   playlist_write_file(playlist);
   playlist_free(playlist);

   database_info_list_free(db_state->info);
   free(db_state->info);

   db_state->info = NULL;
   db_state->crc  = 0;

   free(entry_path_str);
   free(db_playlist_path);
   free(db_playlist_base_str);
   free(db_crc);

   return 0;
}

/* End of entries in database info list and didn't find a
 * match, go to the next database. */
static int database_info_list_iterate_next(
      database_state_handle_t *db_state
      )
{
   db_state->list_index++;
   db_state->entry_index = 0;

   database_info_list_free(db_state->info);
   free(db_state->info);
   db_state->info        = NULL;

   return 1;
}

static int task_database_iterate_crc_lookup(
      db_handle_t *_db,
      database_state_handle_t *db_state,
      database_info_handle_t *db,
      const char *name,
      const char *archive_entry)
{

   if (!db_state->list ||
         (unsigned)db_state->list_index == (unsigned)db_state->list->size)
      return database_info_list_iterate_end_no_match(db_state);

   if (db_state->entry_index == 0)
   {
      bool db_supports_content;
      bool unsupported_content;
      char query[50];

      query[0] = '\0';

      db_supports_content = core_info_database_supports_content_path(
            db_state->list->elems[db_state->list_index].data, name);
      unsupported_content = core_info_unsupported_content_path(name);

      /* don't scan files that can't be in this database */
      if(!db_supports_content || unsupported_content)
         return database_info_list_iterate_next(db_state);

      snprintf(query, sizeof(query),
            "{crc:or(b\"%08X\",b\"%08X\")}",
            db_state->crc, db_state->archive_crc);

      database_info_list_iterate_new(db_state, query);
   }

   if (db_state->info)
   {
      database_info_t *db_info_entry =
         &db_state->info->list[db_state->entry_index];

      if (db_info_entry && db_info_entry->crc32)
      {
#if 0
         RARCH_LOG("CRC32: 0x%08X , entry CRC32: 0x%08X (%s).\n",
               db_state->crc, db_info_entry->crc32, db_info_entry->name);
#endif
         if (db_state->archive_crc == db_info_entry->crc32)
            return database_info_list_iterate_found_match(
                  _db,
                  db_state, db, NULL);
         if (db_state->crc == db_info_entry->crc32)
            return database_info_list_iterate_found_match(
                  _db,
                  db_state, db, archive_entry);
      }
   }

   db_state->entry_index++;

   if (db_state->info)
   {
      if (db_state->entry_index >= db_state->info->count)
         return database_info_list_iterate_next(db_state);
   }

   /* If we haven't reached the end of the database list yet,
    * continue iterating. */
   if (db_state->list_index < db_state->list->size)
      return 1;

   database_info_list_free(db_state->info);

   if (db_state->info)
      free(db_state->info);

   return 0;
}

static int task_database_iterate_playlist_archive(
      db_handle_t *_db,
      database_state_handle_t *db_state,
      database_info_handle_t *db, const char *name)
{
#ifdef HAVE_COMPRESSION
   if (db_state->crc != 0)
      return task_database_iterate_crc_lookup(
            _db, db_state, db, name, db_state->archive_name);

   db_state->crc = file_archive_get_file_crc32(name);
#endif

   return 1;
}

static int task_database_iterate_playlist_lutro(
      db_handle_t *_db,
      database_state_handle_t *db_state,
      database_info_handle_t *db,
      const char *path)
{
   char *db_playlist_path  = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   playlist_t   *playlist  = NULL;

   db_playlist_path[0]     = '\0';

   fill_pathname_join(db_playlist_path,
         _db->playlist_directory,
         file_path_str(FILE_PATH_LUTRO_PLAYLIST),
         PATH_MAX_LENGTH * sizeof(char));

   playlist = playlist_init(db_playlist_path, COLLECTION_SIZE);

   free(db_playlist_path);

   if(!playlist_entry_exists(playlist,
            path, file_path_str(FILE_PATH_DETECT)))
   {
      char *game_title = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

      game_title[0] = '\0';

      fill_short_pathname_representation_noext(game_title,
            path, PATH_MAX_LENGTH * sizeof(char));

      playlist_push(playlist, path,
            game_title,
            file_path_str(FILE_PATH_DETECT),
            file_path_str(FILE_PATH_DETECT),
            file_path_str(FILE_PATH_DETECT),
            file_path_str(FILE_PATH_LUTRO_PLAYLIST));

      free(game_title);
   }

   playlist_write_file(playlist);
   playlist_free(playlist);

   return 0;
}


static int task_database_iterate_serial_lookup(
      db_handle_t *_db,
      database_state_handle_t *db_state,
      database_info_handle_t *db, const char *name)
{
   if (!db_state->list ||
         (unsigned)db_state->list_index == (unsigned)db_state->list->size)
      return database_info_list_iterate_end_no_match(db_state);

   if (db_state->entry_index == 0)
   {
      char query[50];
      char *serial_buf =
         bin_to_hex_alloc((uint8_t*)db_state->serial, 10 * sizeof(uint8_t));

      if (!serial_buf)
         return 1;

      query[0] = '\0';

      snprintf(query, sizeof(query), "{'serial': b'%s'}", serial_buf);
      database_info_list_iterate_new(db_state, query);

      free(serial_buf);
   }

   if (db_state->info)
   {
      database_info_t *db_info_entry = &db_state->info->list[
         db_state->entry_index];

      if (db_info_entry && db_info_entry->serial)
      {
#if 0
         RARCH_LOG("serial: %s , entry serial: %s (%s).\n",
                   db_state->serial, db_info_entry->serial,
                   db_info_entry->name);
#endif
         if (string_is_equal(db_state->serial, db_info_entry->serial))
            return database_info_list_iterate_found_match(_db,
                  db_state, db, NULL);
      }
   }

   db_state->entry_index++;

   if (db_state->info)
   {
      if (db_state->entry_index >= db_state->info->count)
         return database_info_list_iterate_next(db_state);
   }

   /* If we haven't reached the end of the database list yet,
    * continue iterating. */
   if (db_state->list_index < db_state->list->size)
      return 1;

   database_info_list_free(db_state->info);
   free(db_state->info);
   return 0;
}

static int task_database_iterate(
      db_handle_t *_db,
      database_state_handle_t *db_state,
      database_info_handle_t *db)
{
   const char *name = database_info_get_current_element_name(db);

   if (!name)
      return 0;

   if (database_info_get_type(db) == DATABASE_TYPE_ITERATE)
      if (path_contains_compressed_file(name))
         database_info_set_type(db, DATABASE_TYPE_ITERATE_ARCHIVE);

   switch (database_info_get_type(db))
   {
      case DATABASE_TYPE_ITERATE:
         return task_database_iterate_playlist(db_state, db, name);
      case DATABASE_TYPE_ITERATE_ARCHIVE:
         return task_database_iterate_playlist_archive(_db, db_state, db, name);
      case DATABASE_TYPE_ITERATE_LUTRO:
         return task_database_iterate_playlist_lutro(_db, db_state, db, name);
      case DATABASE_TYPE_SERIAL_LOOKUP:
         return task_database_iterate_serial_lookup(_db, db_state, db, name);
      case DATABASE_TYPE_CRC_LOOKUP:
         return task_database_iterate_crc_lookup(_db, db_state, db, name, NULL);
      case DATABASE_TYPE_NONE:
      default:
         break;
   }

   return 0;
}

static void task_database_cleanup_state(
      database_state_handle_t *db_state)
{
   if (!db_state)
      return;

   if (db_state->buf)
      free(db_state->buf);
   db_state->buf = NULL;
}

static void task_database_handler(retro_task_t *task)
{
   const char *name                 = NULL;
   database_info_handle_t  *dbinfo  = NULL;
   database_state_handle_t *dbstate = NULL;
   db_handle_t *db                  = NULL;

   if (!task)
      goto task_finished;

   db      = (db_handle_t*)task->state;

   if (!db)
      goto task_finished;

   if (!db->scan_started)
   {
      db->scan_started = true;

      if (db->is_directory)
         db->handle = database_info_dir_init(db->fullpath, DATABASE_TYPE_ITERATE, task);
      else
         db->handle = database_info_file_init(db->fullpath, DATABASE_TYPE_ITERATE, task);

      task_free_title(task);

      if (db->handle)
         db->handle->status = DATABASE_STATUS_ITERATE_BEGIN;
   }

   dbinfo  = db->handle;
   dbstate = &db->state;

   if (!dbinfo || task_get_cancelled(task))
      goto task_finished;

   switch (dbinfo->status)
   {
      case DATABASE_STATUS_ITERATE_BEGIN:
         if (dbstate && !dbstate->list)
         {
            dbstate->list        = dir_list_new_special(
                  db->content_database_path,
                  DIR_LIST_DATABASES, NULL);
         }
         dbinfo->status = DATABASE_STATUS_ITERATE_START;
         break;
      case DATABASE_STATUS_ITERATE_START:
         name = database_info_get_current_element_name(dbinfo);
         task_database_cleanup_state(dbstate);
         dbstate->list_index  = 0;
         dbstate->entry_index = 0;
         task_database_iterate_start(dbinfo, name);
         break;
      case DATABASE_STATUS_ITERATE:
         if (task_database_iterate(db, dbstate, dbinfo) == 0)
         {
            dbinfo->status = DATABASE_STATUS_ITERATE_NEXT;
            dbinfo->type   = DATABASE_TYPE_ITERATE;
         }
         break;
      case DATABASE_STATUS_ITERATE_NEXT:
         if (task_database_iterate_next(dbinfo) == 0)
         {
            dbinfo->status = DATABASE_STATUS_ITERATE_START;
            dbinfo->type   = DATABASE_TYPE_ITERATE;
         }
         else
         {
            runloop_msg_queue_push(
                  msg_hash_to_str(MSG_SCANNING_OF_DIRECTORY_FINISHED),
                  0, 180, true);
            goto task_finished;
         }
         break;
      default:
      case DATABASE_STATUS_FREE:
      case DATABASE_STATUS_NONE:
         goto task_finished;
   }

   return;
task_finished:
   if (task)
      task_set_finished(task, true);

   if (dbstate)
   {
      if (dbstate->list)
         dir_list_free(dbstate->list);
   }

   if (db)
   {
      if (db->state.buf)
         free(db->state.buf);

      if (db->handle)
         database_info_free(db->handle);
      free(db);
   }

   if (dbinfo)
      free(dbinfo);
}

bool task_push_dbscan(
      const char *playlist_directory,
      const char *content_database,
      const char *fullpath,
      bool directory, retro_task_callback_t cb)
{
   retro_task_t *t      = (retro_task_t*)calloc(1, sizeof(*t));
   db_handle_t *db      = (db_handle_t*)calloc(1, sizeof(db_handle_t));

   if (!t || !db)
      goto error;

   t->handler        = task_database_handler;
   t->state          = db;
   t->callback       = cb;
   t->title          = strdup(msg_hash_to_str(MSG_PREPARING_FOR_CONTENT_SCAN));

   db->is_directory = directory;

   strlcpy(db->fullpath, fullpath, sizeof(db->fullpath));
   strlcpy(db->playlist_directory, playlist_directory,
         sizeof(db->playlist_directory));
   strlcpy(db->content_database_path, content_database,
         sizeof(db->content_database_path));

   task_queue_push(t);

   return true;

error:
   if (t)
      free(t);
   if (db)
      free(db);
   return false;
}

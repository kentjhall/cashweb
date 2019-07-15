/*
 * Copyright 2013 MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef MONGOC_H
#define MONGOC_H


#include "libmongoc/bson/bson.h"

#define MONGOC_INSIDE
#include "libmongoc/mongoc/mongoc-macros.h"
#include "libmongoc/mongoc/mongoc-apm.h"
#include "libmongoc/mongoc/mongoc-bulk-operation.h"
#include "libmongoc/mongoc/mongoc-change-stream.h"
#include "libmongoc/mongoc/mongoc-client.h"
#include "libmongoc/mongoc/mongoc-client-pool.h"
#include "libmongoc/mongoc/mongoc-collection.h"
#include "libmongoc/mongoc/mongoc-config.h"
#include "libmongoc/mongoc/mongoc-cursor.h"
#include "libmongoc/mongoc/mongoc-database.h"
#include "libmongoc/mongoc/mongoc-index.h"
#include "libmongoc/mongoc/mongoc-error.h"
#include "libmongoc/mongoc/mongoc-flags.h"
#include "libmongoc/mongoc/mongoc-gridfs.h"
#include "libmongoc/mongoc/mongoc-gridfs-bucket.h"
#include "libmongoc/mongoc/mongoc-gridfs-file.h"
#include "libmongoc/mongoc/mongoc-gridfs-file-list.h"
#include "libmongoc/mongoc/mongoc-gridfs-file-page.h"
#include "libmongoc/mongoc/mongoc-host-list.h"
#include "libmongoc/mongoc/mongoc-init.h"
#include "libmongoc/mongoc/mongoc-matcher.h"
#include "libmongoc/mongoc/mongoc-handshake.h"
#include "libmongoc/mongoc/mongoc-opcode.h"
#include "libmongoc/mongoc/mongoc-log.h"
#include "libmongoc/mongoc/mongoc-socket.h"
#include "libmongoc/mongoc/mongoc-client-session.h"
#include "libmongoc/mongoc/mongoc-stream.h"
#include "libmongoc/mongoc/mongoc-stream-buffered.h"
#include "libmongoc/mongoc/mongoc-stream-file.h"
#include "libmongoc/mongoc/mongoc-stream-gridfs.h"
#include "libmongoc/mongoc/mongoc-stream-socket.h"
#include "libmongoc/mongoc/mongoc-uri.h"
#include "libmongoc/mongoc/mongoc-write-concern.h"
#include "libmongoc/mongoc/mongoc-version.h"
#include "libmongoc/mongoc/mongoc-version-functions.h"
#ifdef MONGOC_ENABLE_SSL
#include "libmongoc/mongoc/mongoc-rand.h"
#include "libmongoc/mongoc/mongoc-stream-tls.h"
#include "libmongoc/mongoc/mongoc-ssl.h"
#endif
#undef MONGOC_INSIDE


#endif /* MONGOC_H */

/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Dehydra and Treehydra scriptable static analysis tools
 * Copyright (C) 2007-2010 The Mozilla Foundation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef DEHYDRA_BUILTINS_H
#define DEHYDRA_BUILTINS_H

/* Support older SpiderMonkey versions where JSNative != JSFastNative */
#ifdef JSFUN_FAST_NATIVE
#define DH_JS_FN JS_FN
#else
#define DH_JS_FN(a, b, c, d, e) JS_FN(a, b, c, d)
#define JSFUN_FAST_NATIVE 0
#endif

/* JS Natives */

#define DH_JSNATIVE(fname) JSBool fname(JSContext *cx, uintN argc, jsval *vp);
 
DH_JSNATIVE(Require);
DH_JSNATIVE(Include);
 
DH_JSNATIVE(Diagnostic);
DH_JSNATIVE(Print);
 
DH_JSNATIVE(WriteFile);
DH_JSNATIVE(ReadFile);
DH_JSNATIVE(Hashcode);
DH_JSNATIVE(ResolvePath);

void ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report);

/* Related C functions */

char *readFile(const char *path, long *size);
FILE *findFile(const char *filename, const char *dir, char **realname);
char *readEntireFile(FILE *f, long *size);
void  reportError(JSContext *cx, const char *file, int line, 
                  const char *fmt, ...);

#define REPORT_ERROR_0(cx, fmt) reportError(cx, __FILE__, __LINE__, fmt);
#define REPORT_ERROR_1(cx, fmt, arg1) reportError(cx, __FILE__, __LINE__, fmt, arg1);
#define REPORT_ERROR_2(cx, fmt, arg1, arg2) reportError(cx, __FILE__, __LINE__, fmt, arg1, arg2);

#endif

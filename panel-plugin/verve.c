/***************************************************************************
 *            verve.c
 *
 *  Copyright  2006-2007  Jannis Pohlmann
 *  jannis@xfce.org
 *  DuckDuckGo support (C) 2012-13 Isaac Schemm
 *  isaacschemm@gmail.com
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
#include <pcre.h>

#include <glib-object.h>

#include <libxfcegui4/libxfcegui4.h>

#include "verve.h"
#include "verve-env.h"
#include "verve-history.h"



static gboolean verve_is_url       (const gchar *str);
static gboolean verve_is_email     (const gchar *str);
static gboolean verve_is_directory (const gchar *str);



/* URL/eMail matching patterns */
#define USERCHARS   "-A-Za-z0-9"
#define PASSCHARS   "-A-Za-z0-9,?;.:/!%$^*&~\"#'"
#define HOSTCHARS   "-A-Za-z0-9"
#define USER        "[" USERCHARS "]+(:["PASSCHARS "]+)?"
#define MATCH_URL1  "^((file|https?|ftps?)://(" USER "@)?)[" HOSTCHARS ".]+(:[0-9]+)?" \
                    "(/[-A-Za-z0-9_$.+!*(),;:@&=?/~#%]*[^]'.}>) \t\r\n,\\\"])?/?$"
#define MATCH_URL2  "^(www|ftp)[" HOSTCHARS "]*\\.[" HOSTCHARS ".]+(:[0-9]+)?" \
                    "(/[-A-Za-z0-9_$.+!*(),;:@&=?/~#%]*[^]'.}>) \t\r\n,\\\"])?/?$"
#define MATCH_EMAIL "^(mailto:)?[a-z0-9][a-z0-9.-]*@[a-z0-9][a-z0-9-]*(\\.[a-z0-9][a-z0-9-]*)+$"
#define MATCH_BANG  "^!"
#define MATCH_BACKSLASH "^\\\\"

static gboolean use_bang = TRUE;
static gboolean use_backslash = FALSE;
static gboolean use_smartbookmark = FALSE;
/* URL for smartbookmark */
static gchar* url = "";

void
verve_set_bang_setting (gboolean bang)
{
  use_bang = bang;
}

void
verve_set_backslash_setting (gboolean backslash)
{
  use_backslash = backslash;
}

void
verve_set_smartbookmark_setting (gboolean smartbookmark)
{
  use_smartbookmark = smartbookmark;
}

void
verve_set_url (gchar* engine)
{
  url = g_strdup(engine);
}

/*********************************************************************
 *
 * Initialize/shutdown Verve
 * -------------------------
 *
 * The execution of these functions is necessary before and after 
 * your program makes use of Verve.
 *
 *********************************************************************/

void 
verve_init (void)
{
  /* Init history database */
  verve_history_init ();
}



void
verve_shutdown (void)
{
  /* Free history database */
  verve_history_shutdown ();

  /* Shutdown environment */
  verve_env_shutdown ();
}



/*********************************************************************
 *
 * Verve command line execution function
 * -------------------------------------
 *
 * With the help of this function, shell commands can be executed.
 *
 *********************************************************************/
 
gboolean verve_spawn_command_line (const gchar *cmdline)
{
  gint         argc;
  gchar      **argv;
  gboolean     success;
  GError      *error = NULL;
  const gchar *home_dir;
  GSpawnFlags  flags;
  
  /* Parse command line arguments */
  success = g_shell_parse_argv (cmdline, &argc, &argv, &error);

  /* Return false if command line arguments failed to be arsed */
  if (G_UNLIKELY (error != NULL))
    {
      g_error_free (error);
      return FALSE;
    }

  /* Get user's home directory */
  home_dir = xfce_get_homedir ();

  /* Set up spawn flags */
  flags = G_SPAWN_STDOUT_TO_DEV_NULL;
  flags |= G_SPAWN_STDERR_TO_DEV_NULL;
  flags |= G_SPAWN_SEARCH_PATH;
  
  /* Spawn subprocess */
  success = g_spawn_async (home_dir, argv, NULL, flags, NULL, NULL, NULL, &error);

  /* Return false if subprocess could not be spawned */
  if (G_UNLIKELY (error != NULL)) 
    {
      g_error_free (error);
      return FALSE;
    }

  /* Free command line arguments */
  g_strfreev (argv);
  
  /* Return whether process was spawned successfully */
  return success;
}



/*********************************************************************
 * 
 * Verve main execution method
 * ---------------------------
 *
 * This method should be used whenever you want to run command line 
 * input, be it a URL, an eMail address or a custom command.
 *
 *********************************************************************/

gboolean
verve_execute (const gchar *input, 
               gboolean terminal)
{
  gchar   *command;
  gboolean result = FALSE;
    
  /* Open URLs, eMail addresses and directories using exo-open */
  if (verve_is_url (input) || verve_is_email (input) || verve_is_directory (input))
  {
    /* Build exo-open command */
    command = g_strconcat ("exo-open ", input, NULL);
  }
  else
  {
    if ((use_bang && verve_is_bang (input)) || (use_backslash && verve_is_backslash (input)))
    {
      /* Launch DuckDuckGo */
      gchar* esc_input = g_strescape(input, NULL);
      command = g_strconcat ("verve-search-launcher https://duckduckgo.com/?q=", esc_input, NULL);
      g_free(esc_input);
    }
    else
    {
      if (use_smartbookmark)
      {
        /* Launch user-defined search engine */
        command = g_strconcat ("verve-search-launcher ", url, input, NULL);
      }
      else
      {
        /* Run command using the xfterm4 wrapper if the terminal flag was set */
        if (G_UNLIKELY (terminal))
          command = g_strconcat ("xfterm4 -e ", input, NULL);
        else
          command = g_strdup (input);
      }
    }
  }
    
  /* Try to execute the exo-open command */
  if (verve_spawn_command_line (command))
    result = TRUE;
    
  /* Free command string */
  g_free (command);

  /* Return spawn result */
  return result;
}



/*********************************************************************
 *
 * Internal pattern matching functions
 *
 *********************************************************************/

gboolean
verve_is_url (const gchar *str)
{
  GString     *string = g_string_new (str);
  pcre        *pattern;
  const gchar *error;
  int          error_offset;
  int          ovector[30];
  gboolean     success = FALSE;

  /* Compile first pattern */
  pattern = pcre_compile (MATCH_URL1, 0, &error, &error_offset, NULL);
  
  /* Test whether the string matches this pattern */
  if (pcre_exec (pattern, NULL, string->str, string->len, 0, 0, ovector, 30) >= 0)
    success = TRUE;

  /* Free pattern */
  pcre_free (pattern);

  /* Free data and return true if string matched the first pattern */
  if (success)
    {
      g_string_free (string, TRUE);
      return TRUE;
    }

  /* Compile second pattern */
  pattern = pcre_compile (MATCH_URL2, 0, &error, &error_offset, NULL);

  /* Test whether the string matches this pattern */
  if (pcre_exec (pattern, NULL, string->str, string->len, 0, 0, ovector, 30) >= 0)
    success = TRUE;

  /* Free pattern */
  pcre_free (pattern);

  /* Free string */
  g_string_free (string, TRUE);

  /* Return whether the string matched any of the URL patterns */
  return success;
}



gboolean
verve_is_directory (const gchar *str)
{
  /* Avoid opening directories with the same name as an existing executable. */
  if (g_find_program_in_path (str))
    return FALSE;

  if (g_file_test (str, G_FILE_TEST_IS_DIR))
    return TRUE;
  else
    return FALSE;
}



gboolean
verve_match_regex (const gchar *str, const gchar *regex)
{
  GString     *string = g_string_new (str);
  pcre        *pattern;
  const gchar *error;
  int          error_offset;
  int          ovector[30];
  gboolean     success = FALSE;

  /* Compile pattern */
  pattern = pcre_compile (regex, 0, &error, &error_offset, NULL);

  /* Test whether the string matches this pattern */
  if (pcre_exec (pattern, NULL, string->str, string->len, 0, 0, ovector, 30) >= 0)
    success = TRUE;

  /* Free pattern and string */
  pcre_free (pattern);
  g_string_free (string, TRUE);

  if (success)
    {
      /* Don't search DuckDuckGo for this query is there is an executable by this name. */
      if (g_find_program_in_path (str))
      {
        success = FALSE;
      }
    }

  /* Return true if string matched the first pattern and there is no matching executable */
  return success;
}



gboolean
verve_is_email (const gchar *str)
{
  return verve_match_regex(str, MATCH_EMAIL);
}



gboolean
verve_is_bang(const gchar *str)
{
  return verve_match_regex(str, MATCH_BANG);
}



gboolean
verve_is_backslash(const gchar *str)
{
  return verve_match_regex(str, MATCH_BACKSLASH);
}



/* vim:set expandtab sts=2 ts=2 sw=2: */

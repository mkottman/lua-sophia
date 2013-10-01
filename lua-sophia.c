/*** lua-sophia, a Lua binding to the [Sophia](http://sphia.org) embeddable key-value database.
The database stores only Lua string keys and values, for storing more complex types
use a serialization library, such as MessagePack.

Usage:
	local sophia = require "sophia"
	local env = sophia.env() -- create a new environment
	env:ctl("dir", "db") -- set the directory where the database is/will be stored
	
	local db = ctl:open() -- open the database handle

	-- store and retrieve a value
	db:set("greeting", "Hello world!")
	print(db:get("greeting")) --> Hello world!

@module sophia
@author Michal Kottman
*/

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <string.h>
#include <sophia.h>

#define META_ENV "Sophia Environment"
#define META_DB "Sophia Database"
#define META_CUR "Sophia Cursor"

#if LUA_VERSION_NUM < 502
#  define luaL_newlib(L,l) (lua_newtable(L), luaL_register(L,NULL,l))
#endif

/* Workaround while upstream does not fix the default key comparison functions */
static inline int cmp(char *a, size_t asz, char *b, size_t bsz, void *arg)
{
	register size_t sz = (asz < bsz ? asz : bsz);
	register int rc = memcmp(a, b, sz);
	return (rc == 0 ?
		/* first `sz` bytes are the same, compare lengths */
		((asz == bsz) ? 0 : (asz < bsz ? -1 : 1)) 
		:
		/* they differ, normalize to -1 : 1 */
		(rc > 0 ? 1 : -1));
}

/** Create a new Sophia environment handle. This can be used to open database handles. 
@function sophia.env
@treturn environment A new environment
*/
static int ls_env(lua_State *L)
{
	void *env = sp_env();
	void ** ud;

	if (env == NULL) {
		lua_pushnil(L);
		lua_pushliteral(L, "Failed to create Sophia environment");
		return 2;
	}
	{
		int rc = sp_ctl(env, SPCMP, cmp, NULL);
		if (rc == -1) {
			lua_pushnil(L);
			lua_pushstring(L, sp_error(env));
			sp_destroy(env);
			return 2;
		}
	}

	ud = (void**) lua_newuserdata(L, sizeof(void*));
	*ud = env;
	luaL_getmetatable(L, META_ENV);
	lua_setmetatable(L, -2);
	return 1;
}

static int ls_destroy(lua_State *L)
{
	void **obj = (void**) lua_touserdata(L, 1);
	if (*obj) {
		sp_destroy(*obj);
		*obj = NULL;
	}
	return 0;
}


/** @type environment */

/** Configure a database.
Currently only `"dir"` option is supported. This needs to be run before `environment:open()` to
set the directory where data is stored. Example: `env:ctl("dir", "db-directory")`

@function environment:ctl
@tparam string option An option to configure
@param ... Additional data for the given option
@return **true** on success, **nil, message** on error
*/
static int ls_ctl(lua_State *L)
{
	void * ud = luaL_checkudata(L, 1, META_ENV);
	void * env = *(void**) ud;
	const char *options[] = { "dir", NULL };
	int opt = luaL_checkoption(L, 2, NULL, options);
	int rc = 0;
	
	if (opt == 0) {
		const char *dir = luaL_checkstring(L, 3);
		rc = sp_ctl(env, SPDIR, SPO_CREAT|SPO_RDWR, dir);
	}

	if (rc == -1) {
		lua_pushnil(L);
		lua_pushstring(L, sp_error(env));
		return 2;
	}
	lua_pushboolean(L, 1);
	return 1;
}

/** Open a database. You need to set the database directory by using `environment:ctl("dir")`
before opening the database.

@function environment:open
@treturn database on success, **nil, message** on error
*/
static int ls_open(lua_State *L)
{
	void * ud = luaL_checkudata(L, 1, META_ENV);
	void * env = *(void**) ud;

	void * db = sp_open(env);
	void ** uddb;

	if (db == NULL) {
		lua_pushnil(L);
		lua_pushstring(L, sp_error(env));
		return 2;
	}

	uddb = (void**) lua_newuserdata(L, sizeof(void*));
	*uddb = db;
	luaL_getmetatable(L, META_DB);
	lua_setmetatable(L, -2);
	return 1;
}


/** @type database */


/** Begin a transaction.

During transaction, all updates are not written to the database files
until a @{database:commit} is called. All updates made during transaction are
available through @{database:get} or by using cursor.

@function database:begin
@return **true** on success, **nil, message** on error
*/
static int ls_begin(lua_State *L)
{
	void * ud = luaL_checkudata(L, 1, META_DB);
	void * db = *(void**) ud;

	int ret = sp_begin(db);
	if (ret == -1) {
		lua_pushnil(L);
		lua_pushstring(L, sp_error(db));
	}
	lua_pushboolean(L, 1);
	return 1;
}

/** Commit a transaction.

This function is used to apply changes of a multi-statement
transaction. All modifications made during the transaction are written to 
the log file in a single batch. If commit failed, transaction modifications are discarded.

@function database:commit
@return **true** on success, **nil, message** on error
*/
static int ls_commit(lua_State *L)
{
	void * ud = luaL_checkudata(L, 1, META_DB);
	void * db = *(void**) ud;

	int ret = sp_commit(db);
	if (ret == -1) {
		lua_pushnil(L);
		lua_pushstring(L, sp_error(db));
	}
	lua_pushboolean(L, 1);
	return 1;
}

/** Rollback a transaction.

This function is used to discard a changes of a multi-statement
transaction. All modifications made during the transaction are not written to 
the log file.

@function database:rollback
@return **true** on success, **nil, message** on error
*/
static int ls_rollback(lua_State *L)
{
	void * ud = luaL_checkudata(L, 1, META_DB);
	void * db = *(void**) ud;

	int ret = sp_rollback(db);
	if (ret == -1) {
		lua_pushnil(L);
		lua_pushstring(L, sp_error(db));
	}
	lua_pushboolean(L, 1);
	return 1;
}


/** Retrieve a value from the database.

If the `key` is not in the database, it return **nil**, as in standard Lua table lookup.
In case of error while retrieving the value, it returns an extra error message.

@function database:get
@tparam string key The key to lookup in the database
@treturn[1] string value of `key` if found in database
@return[2] **nil** if the value is not in the database
@return[3] **nil, message** in case there was an error retrieving the value 
*/
static int ls_get(lua_State *L)
{
	void * ud = luaL_checkudata(L, 1, META_DB);
	void * db = *(void**) ud;
	size_t ksize = 0;
	const char * key = luaL_checklstring(L, 2, &ksize);
	void * value;
	size_t vsize;

	int rc = sp_get(db, key, ksize, &value, &vsize);
	
	if (rc == -1) {
		lua_pushnil(L);
		lua_pushstring(L, sp_error(db));
		return 2;
	} else if (rc == 0) {
		lua_pushnil(L);
		return 1;
	} else {
		lua_pushlstring(L, (const char *) value, vsize);
		free(value);
		return 1;		
	}
}

/** Set or delete a value in the database.

The `value` is stored in the database under `key`. If `value` is **nil**,
the given key is deleted from the database.

@function database:set
@tparam string key The key under which to store the value
@tparam string value The value to store in database. 
@return[1] **true** if the value was stored
@return[2] **nil, message** in case there was an error retrieving the value 
*/
static int ls_set(lua_State *L)
{
	void * ud = luaL_checkudata(L, 1, META_DB);
	void * db = *(void**) ud;
	size_t ksize = 0;
	const char * key = luaL_checklstring(L, 2, &ksize);
	int rc = 0;

	if (lua_isnoneornil(L, 3)) {
		rc = sp_delete(db, key, ksize);
	} else {
		size_t vsize = 0;
		const char * value = luaL_checklstring(L, 3, &vsize);
		rc = sp_set(db, key, ksize, value, vsize);
	}

	if (rc == -1) {
		lua_pushnil(L);
		lua_pushstring(L, sp_error(db));
		return 2;		
	}

	lua_pushboolean(L, 1);
	return 1;
}

static int ls_fetch(lua_State *L)
{
	void * ud = luaL_checkudata(L, 1, META_CUR);
	void * cur = *(void**) ud;

	int more = sp_fetch(cur);
	if (more) {
		lua_pushlstring(L, sp_key(cur), sp_keysize(cur));
		lua_pushlstring(L, sp_value(cur), sp_valuesize(cur));
		return 2;
	} else {
		sp_destroy(cur);
		/* prevent destruction by __gc */
		(*(void**) ud) = NULL;
		return 0;
	}
}

/** Iterate over the keys and values in the database.

This function returns values necessary for the `for` Lua iterator. Example:

    for k, v in db:cursor() do
    	print(k, v)
    end

@function database:cursor
@return values necessary for the `for` iterator
*/
static int ls_cursor(lua_State *L)
{
	void * ud = luaL_checkudata(L, 1, META_DB);
	void * db = *(void**) ud;
	size_t ksize = 0;
	const char * key = NULL;
	void * cur;
	void ** udcur;
	
	if (lua_type(L, 2) == LUA_TSTRING) {
		key = lua_tolstring(L, 2, &ksize);
	}
	cur = sp_cursor(db, SPGTE, key, ksize);
	if (cur == NULL) {
		lua_pushnil(L);
		lua_pushstring(L, sp_error(db));
		return 2;	
	}

	lua_pushcfunction(L, ls_fetch);

	udcur = (void**) lua_newuserdata(L, sizeof(void*));
	*udcur = cur;
	luaL_getmetatable(L, META_CUR);
	lua_setmetatable(L, -2);

	lua_pushnil(L);
	return 3;
}


static luaL_Reg sophia_module[] = {
	{"env", ls_env},
	{NULL, NULL},
};

static luaL_Reg sophia_env_methods[] = {
	{"__gc", ls_destroy},
	{"ctl", ls_ctl},
	{"open", ls_open},
	{NULL, NULL},
};

static luaL_Reg sophia_db_methods[] = {
	{"__gc", ls_destroy},
	{"begin", ls_begin},
	{"commit", ls_commit},
	{"rollback", ls_rollback},
	{"get", ls_get},
	{"set", ls_set},
	{"cursor", ls_cursor},
	{"__pairs", ls_cursor},
	{NULL, NULL},
};

static luaL_Reg sophia_cur_methods[] = {
	{"__gc", ls_destroy},
	{NULL, NULL},
};

static void create_meta(lua_State *L, const char * name, luaL_Reg* methods)
{
	luaL_newmetatable(L, name);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	luaL_register(L, NULL, methods);
}

int luaopen_sophia(lua_State *L)
{
	create_meta(L, META_ENV, sophia_env_methods);
	create_meta(L, META_DB, sophia_db_methods);
	create_meta(L, META_CUR, sophia_cur_methods);
	luaL_newlib(L, sophia_module);
	return 1;	
}
#include "stdafx.h"
#include "rpc_server.h"
#include "gdipp_lib/helper.h"
#include "gdipp_lib/scoped_rw_lock.h"
#include "gdipp_lib/scoped_rw_lock.h"
#include "gdipp_rpc/gdipp_rpc.h"
#include "gdipp_server/freetype.h"
#include "gdipp_server/ft_renderer.h"
#include "gdipp_server/ggo_renderer.h"
#include "gdipp_server/global.h"
#include "gdipp_server/helper.h"

namespace gdipp
{

HANDLE process_heap = GetProcessHeap();

/*
// rpc index functions

bool rpc_index_initialize()
{
	int i_ret;

	i_ret = sqlite3_initialize();
	if (i_ret != SQLITE_OK)
		return false;

	// open SQLite database in memory
	i_ret = sqlite3_open(":memory:", &index_db_instance);
	if (i_ret != SQLITE_OK)
		return false;

	// create index tables
	i_ret = sqlite3_exec(index_db_instance, "CREATE TABLE session_index ('session_address' INTEGER NOT NULL)", NULL, NULL, NULL);
	if (i_ret != SQLITE_OK)
		return false;

	i_ret = sqlite3_exec(index_db_instance, "CREATE TABLE glyph_run_index ('glyph_run_address' INTEGER NOT NULL)", NULL, NULL, NULL);
	if (i_ret != SQLITE_OK)
		return false;

	return true;
}

bool rpc_index_shutdown()
{
	return (sqlite3_shutdown() == SQLITE_OK);
}

const rpc_session *rpc_index_lookup_session(GDIPP_RPC_SESSION_HANDLE h_session)
{
	int i_ret;

	const int session_id = reinterpret_cast<int>(h_session);
	const rpc_session *curr_session = NULL;

	sqlite3_stmt *select_stmt;

	i_ret = sqlite3_prepare_v2(index_db_instance, "SELECT session_address FROM session_index WHERE ROWID = ?", -1, &select_stmt, NULL);
	assert(i_ret == SQLITE_OK);

	i_ret = sqlite3_bind_int(select_stmt, 0, session_id);
	assert(i_ret == SQLITE_OK);

	i_ret = sqlite3_step(select_stmt);
	if (i_ret == SQLITE_ROW)
	{
#ifdef _M_X64
		curr_session = reinterpret_cast<const rpc_session *>(sqlite3_column_int64(select_stmt, 0));
#else
		curr_session = reinterpret_cast<const rpc_session *>(sqlite3_column_int(select_stmt, 0));
#endif
	}

	i_ret = sqlite3_finalize(select_stmt);
	assert(i_ret == SQLITE_OK);

	return curr_session;
}

const glyph_run *rpc_index_lookup_glyph_run(GDIPP_RPC_GLYPH_RUN_HANDLE h_glyph_run)
{
	int i_ret;

	const int glyph_run_id = reinterpret_cast<int>(h_glyph_run);
	const glyph_run *curr_glyph_run = NULL;

	sqlite3_stmt *select_stmt;

	i_ret = sqlite3_prepare_v2(index_db_instance, "SELECT glyph_run_address FROM glyph_run_index WHERE ROWID = ?", -1, &select_stmt, NULL);
	assert(i_ret == SQLITE_OK);

	i_ret = sqlite3_bind_int(select_stmt, 0, glyph_run_id);
	assert(i_ret == SQLITE_OK);

	i_ret = sqlite3_step(select_stmt);
	if (i_ret == SQLITE_ROW)
	{
#ifdef _M_X64
		curr_glyph_run = reinterpret_cast<const glyph_run *>(sqlite3_column_int64(select_stmt, 0));
#else
		curr_glyph_run = reinterpret_cast<const glyph_run *>(sqlite3_column_int(select_stmt, 0));
#endif
	}

	i_ret = sqlite3_finalize(select_stmt);
	assert(i_ret == SQLITE_OK);

	return curr_glyph_run;
}*/

DWORD WINAPI start_gdipp_rpc_server(LPVOID lpParameter)
{
	if (process_heap == NULL)
		return 1;

	//bool b_ret;
	RPC_STATUS rpc_status;

	scoped_rw_lock::initialize();
	server_cache_size = min(config_instance.get_number(L"/gdipp/server/cache_size/text()", server_config::CACHE_SIZE), 24);
	glyph_cache_instance.initialize();
	initialize_freetype();

	//b_ret = rpc_index_initialize();
	//if (!b_ret)
	//	return 1;

	rpc_status = RpcServerUseProtseqEpW(reinterpret_cast<RPC_WSTR>(L"ncalrpc"), RPC_C_PROTSEQ_MAX_REQS_DEFAULT, reinterpret_cast<RPC_WSTR>(L"gdipp"), NULL);
	if (rpc_status != RPC_S_OK)
		return 1;

	rpc_status = RpcServerRegisterIf(gdipp_rpc_v1_0_s_ifspec, NULL, NULL);
	if (rpc_status != RPC_S_OK)
		return 1;

	rpc_status = RpcServerListen(1, RPC_C_LISTEN_MAX_CALLS_DEFAULT, TRUE);
	if (rpc_status != RPC_S_OK)
		return 1;

	rpc_status = RpcMgmtWaitServerListen();
	if (rpc_status != RPC_S_OK)
		return 1;

	return 0;
}

bool stop_gdipp_rpc_server()
{
	//bool b_ret;
	RPC_STATUS rpc_status;

	rpc_status = RpcMgmtStopServerListening(NULL);
	if (rpc_status != RPC_S_OK)
		return false;

	//b_ret = rpc_index_shutdown();
	//assert(b_ret);

	destroy_freetype();

	return true;
}

}

void __RPC_FAR *__RPC_USER MIDL_user_allocate(size_t size)
{
	return HeapAlloc(gdipp::process_heap, HEAP_GENERATE_EXCEPTIONS, size);
}

void __RPC_USER MIDL_user_free(void __RPC_FAR *ptr)
{
	HeapFree(gdipp::process_heap, 0, ptr);
}

/* [fault_status][comm_status] */ error_status_t gdipp_rpc_begin_session( 
	/* [in] */ handle_t h_gdipp_rpc,
	/* [size_is][in] */ const byte *logfont_buf,
	/* [in] */ unsigned long logfont_size,
	/* [in] */ unsigned short bits_per_pixel,
	/* [out] */ GDIPP_RPC_SESSION_HANDLE *h_session)
{
	if (logfont_size != sizeof(LOGFONTW))
		return RPC_S_INVALID_ARG;

	const HDC session_font_holder = gdipp::dc_pool_instance.claim();
	assert(session_font_holder != NULL);

	// register font with given LOGFONT structure
	const LOGFONTW *logfont = reinterpret_cast<const LOGFONTW *>(logfont_buf);
	BYTE *outline_metrics_buf;
	unsigned long outline_metrics_size;

	void *session_font_id = gdipp::font_mgr_instance.register_font(session_font_holder, logfont, &outline_metrics_buf, &outline_metrics_size);
	if (session_font_id == NULL)
	{
		gdipp::dc_pool_instance.free(session_font_holder);
		return RPC_S_INVALID_ARG;
	}

	const OUTLINETEXTMETRICW *outline_metrics = reinterpret_cast<const OUTLINETEXTMETRICW *>(outline_metrics_buf);
	// generate config trait and retrieve font-specific config
	const LONG point_size = (logfont->lfHeight > 0 ? logfont->lfHeight : -MulDiv(logfont->lfHeight, 72, outline_metrics->otmTextMetrics.tmDigitizedAspectY));
	const char weight_class = gdipp::get_gdi_weight_class(static_cast<unsigned short>(outline_metrics->otmTextMetrics.tmWeight));
	const gdipp::render_config_static *session_render_config = gdipp::font_render_config_cache_instance.get_font_render_config(!!weight_class,
		!!outline_metrics->otmTextMetrics.tmItalic,
		point_size,
		metric_face_name(outline_metrics));
	if (session_render_config->renderer == gdipp::server_config::RENDERER_CLEARTYPE)
	{
		gdipp::dc_pool_instance.free(session_font_holder);
		return RPC_S_OK;
	}

	FT_Render_Mode session_render_mode;
	if (!gdipp::get_render_mode(session_render_config->render_mode, bits_per_pixel, logfont->lfQuality, &session_render_mode))
	{
		gdipp::dc_pool_instance.free(session_font_holder);
		return RPC_S_INVALID_ARG;
	}

	gdipp::rpc_session *new_session = reinterpret_cast<gdipp::rpc_session *>(MIDL_user_allocate(sizeof(gdipp::rpc_session)));

	new_session->bits_per_pixel = bits_per_pixel;
	new_session->font_holder = session_font_holder;
	new_session->font_id = session_font_id;
	new_session->log_font = *reinterpret_cast<const LOGFONTW *>(logfont_buf);
	new_session->outline_metrics_buf = outline_metrics_buf;
	new_session->outline_metrics_size = outline_metrics_size;
	new_session->render_config = session_render_config;
	new_session->render_mode = session_render_mode;
	new_session->render_trait = gdipp::generate_render_trait(logfont, new_session->render_mode);

	// create session renderer
	switch (session_render_config->renderer)
	{
	case gdipp::server_config::RENDERER_DIRECTWRITE:
		//break;
	case gdipp::server_config::RENDERER_FREETYPE:
		new_session->renderer = new gdipp::ft_renderer(new_session);
		break;
	case gdipp::server_config::RENDERER_GETGLYPHOUTLINE:
		new_session->renderer = new gdipp::ggo_renderer(new_session);
		break;
	case gdipp::server_config::RENDERER_WIC:
		break;
	default:
		break;
	}

	*h_session = reinterpret_cast<GDIPP_RPC_SESSION_HANDLE>(new_session);
	return RPC_S_OK;
}

/* [fault_status][comm_status] */ error_status_t gdipp_rpc_get_font_size( 
	/* [in] */ handle_t h_gdipp_rpc,
	/* [context_handle_noserialize][in] */ GDIPP_RPC_SESSION_HANDLE h_session,
	/* [in] */ unsigned long table,
	/* [in] */ unsigned long offset,
	/* [out] */ unsigned long *font_size)
{
	const gdipp::rpc_session *curr_session = reinterpret_cast<const gdipp::rpc_session *>(h_session);
	*font_size = GetFontData(curr_session->font_holder, table, offset, NULL, 0);

	return RPC_S_OK;
}

/* [fault_status][comm_status] */ error_status_t gdipp_rpc_get_font_data( 
	/* [in] */ handle_t h_gdipp_rpc,
	/* [context_handle_noserialize][in] */ GDIPP_RPC_SESSION_HANDLE h_session,
	/* [in] */ unsigned long table,
	/* [in] */ unsigned long offset,
	/* [size_is][out] */ byte *data_buf,
	/* [in] */ unsigned long buf_size)
{
	const gdipp::rpc_session *curr_session = reinterpret_cast<const gdipp::rpc_session *>(h_session);

	// TODO: output pointer is not allocated with MIDL_user_allocate
	// TODO: return value not returned
	GetFontData(curr_session->font_holder, table, offset, data_buf, buf_size);

	return RPC_S_OK;
}

/* [fault_status][comm_status] */ error_status_t gdipp_rpc_get_font_metrics_size( 
	/* [in] */ handle_t h_gdipp_rpc,
	/* [context_handle_noserialize][in] */ GDIPP_RPC_SESSION_HANDLE h_session,
	/* [out] */ unsigned long *metrics_size)
{
	const gdipp::rpc_session *curr_session = reinterpret_cast<const gdipp::rpc_session *>(h_session);
	*metrics_size = curr_session->outline_metrics_size;

	return RPC_S_OK;
}

/* [fault_status][comm_status] */ error_status_t gdipp_rpc_get_font_metrics_data( 
	/* [in] */ handle_t h_gdipp_rpc,
	/* [context_handle_noserialize][in] */ GDIPP_RPC_SESSION_HANDLE h_session,
	/* [size_is][out] */ byte *metrics_buf,
	/* [in] */ unsigned long buf_size)
{
	const gdipp::rpc_session *curr_session = reinterpret_cast<const gdipp::rpc_session *>(h_session);
	const DWORD copy_size = min(curr_session->outline_metrics_size, buf_size);
	CopyMemory(metrics_buf, curr_session->outline_metrics_buf, copy_size);

	return RPC_S_OK;
}

/* [fault_status][comm_status] */ error_status_t gdipp_rpc_get_glyph_indices( 
	/* [in] */ handle_t h_gdipp_rpc,
	/* [context_handle_noserialize][in] */ GDIPP_RPC_SESSION_HANDLE h_session,
	/* [size_is][string][in] */ const wchar_t *str,
	/* [in] */ int count,
	/* [size_is][out] */ unsigned short *gi)
{
	const gdipp::rpc_session *curr_session = reinterpret_cast<const gdipp::rpc_session *>(h_session);

	// TODO: output pointer is not allocated with MIDL_user_allocate
	// TODO: return value not returned
	GetGlyphIndices(curr_session->font_holder, str, count, gi, GGI_MARK_NONEXISTING_GLYPHS);

	return RPC_S_OK;
}

/* [fault_status][comm_status] */ error_status_t gdipp_rpc_make_bitmap_glyph_run( 
	/* [in] */ handle_t h_gdipp_rpc,
	/* [context_handle_noserialize][in] */ GDIPP_RPC_SESSION_HANDLE h_session,
	/* [string][in] */ const wchar_t *string,
	/* [in] */ unsigned int count,
	/* [in] */ boolean is_glyph_index,
	/* [out] */ gdipp_rpc_bitmap_glyph_run *glyph_run_ptr)
{
	if (count == 0)
		return RPC_S_INVALID_ARG;

	const gdipp::rpc_session *curr_session = reinterpret_cast<const gdipp::rpc_session *>(h_session);
	bool b_ret;

	// generate unique identifier for the string
	const gdipp::glyph_cache::string_id_type string_id = gdipp::glyph_cache::get_string_id(string, count, !!is_glyph_index);

	// check if a glyph run cached for the same rendering environment and string
	const gdipp::glyph_run *glyph_run = gdipp::glyph_cache_instance.lookup_glyph_run(string_id, curr_session->render_trait);
	if (!glyph_run)
	{
		// no cached glyph run. render new glyph run
		gdipp::glyph_run *new_glyph_run = new gdipp::glyph_run();
		b_ret = curr_session->renderer->render(!!is_glyph_index, string, count, new_glyph_run);
		if (!b_ret)
			return RPC_S_OUT_OF_MEMORY;

		// and cache it
		gdipp::glyph_cache_instance.store_glyph_run(string_id, curr_session->render_trait, new_glyph_run);
		glyph_run = new_glyph_run;
	}

	// convert internal glyph run to RPC exchangable format

	// allocate space for glyph run
	glyph_run_ptr->count = static_cast<UINT>(glyph_run->glyphs.size());
	glyph_run_ptr->glyphs = reinterpret_cast<gdipp_rpc_bitmap_glyph *>(MIDL_user_allocate(sizeof(gdipp_rpc_bitmap_glyph) * glyph_run_ptr->count));
	glyph_run_ptr->ctrl_boxes = reinterpret_cast<RECT *>(MIDL_user_allocate(sizeof(RECT) * glyph_run_ptr->count));
	glyph_run_ptr->black_boxes = reinterpret_cast<RECT *>(MIDL_user_allocate(sizeof(RECT) * glyph_run_ptr->count));
	glyph_run_ptr->render_mode = curr_session->render_mode;

	for (unsigned int i = 0; i < glyph_run_ptr->count; ++i)
	{
		glyph_run_ptr->ctrl_boxes[i] = glyph_run->ctrl_boxes[i];
		glyph_run_ptr->black_boxes[i] = glyph_run->black_boxes[i];

		if (glyph_run->glyphs[i] == NULL)
		{
			glyph_run_ptr->glyphs[i].buffer = NULL;
			continue;
		}

		const FT_BitmapGlyph bmp_glyph = reinterpret_cast<const FT_BitmapGlyph>(glyph_run->glyphs[i]);
		glyph_run_ptr->glyphs[i].left = bmp_glyph->left;
		glyph_run_ptr->glyphs[i].top = bmp_glyph->top;
		glyph_run_ptr->glyphs[i].rows = bmp_glyph->bitmap.rows;
		glyph_run_ptr->glyphs[i].width = bmp_glyph->bitmap.width;
		glyph_run_ptr->glyphs[i].pitch = bmp_glyph->bitmap.pitch;
		const int buffer_size = bmp_glyph->bitmap.rows * abs(bmp_glyph->bitmap.pitch);
		glyph_run_ptr->glyphs[i].buffer = reinterpret_cast<byte *>(MIDL_user_allocate(buffer_size));
		memcpy(glyph_run_ptr->glyphs[i].buffer, bmp_glyph->bitmap.buffer, buffer_size);
	}

	return RPC_S_OK;
}

/* [fault_status][comm_status] */ error_status_t gdipp_rpc_make_outline_glyph_run( 
	/* [in] */ handle_t h_gdipp_rpc,
	/* [context_handle_noserialize][in] */ GDIPP_RPC_SESSION_HANDLE h_session,
	/* [string][in] */ const wchar_t *string,
	/* [in] */ unsigned int count,
	/* [in] */ boolean is_glyph_index,
	/* [out] */ gdipp_rpc_outline_glyph_run *glyph_run_ptr)
{
	return ERROR_CALL_NOT_IMPLEMENTED;
}

error_status_t gdipp_rpc_end_session( 
    /* [in] */ handle_t h_gdipp_rpc,
    /* [out][in] */ GDIPP_RPC_SESSION_HANDLE *h_session)
{
	const gdipp::rpc_session *curr_session = reinterpret_cast<const gdipp::rpc_session *>(*h_session);
	if (curr_session == NULL)
		return RPC_S_INVALID_ARG;

	delete[] curr_session->outline_metrics_buf;
	delete curr_session->renderer;
	gdipp::dc_pool_instance.free(curr_session->font_holder);
	MIDL_user_free(*h_session);

	*h_session = NULL;
	return RPC_S_OK;
}

void __RPC_USER GDIPP_RPC_SESSION_HANDLE_rundown(GDIPP_RPC_SESSION_HANDLE h_session)
{
	error_status_t e = gdipp_rpc_end_session(NULL, &h_session);
	assert(e == 0);
}

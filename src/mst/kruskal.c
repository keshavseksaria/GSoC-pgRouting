/*PGR-GNU*****************************************************************
File: kruskal.c
Generated with Template by:

Copyright (c) 2015 pgRouting developers
Mail: project@pgrouting.org

Function's developer:
Copyright (c) 2018 Aditya Pratap Singh
Mail: adityapratap.singh28@gmail.com
------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
********************************************************************PGR-GNU*/

#include <stdbool.h>
#include "c_common/postgres_connection.h"
#include <utils/array.h>

#include "c_common/debug_macro.h"
#include "c_common/e_report.h"
#include "c_common/time_msg.h"
#include "c_common/edges_input.h"
#include "c_common/arrays_input.h"


#include "drivers/mst/kruskal_driver.h"

PGDLLEXPORT Datum kruskal(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(kruskal);

static
int
get_order(char * order_by) {
    int order = tolower(order_by[0]);
    if ('d' == order) return 1;
    if ('b' == order) return 2;
    return 0;
}

static
void
process(
        char* edges_sql,
        ArrayType *roots,
        char * p_order_by,
        int64_t max_depth,
        double distance,

        pgr_kruskal_t **result_tuples,
        size_t *result_count) {
    int order_by = get_order(p_order_by);

    int64_t* rootsArr = NULL;
    size_t size_rootsArr = 0;

    PGR_DBG("order by %d", order_by);
    PGR_DBG("max_depth %d", max_depth);

    pgr_SPI_connect();

    rootsArr = (int64_t*) pgr_get_bigIntArray(&size_rootsArr, roots);

    (*result_tuples) = NULL;
    (*result_count) = 0;

    PGR_DBG("Load data");
    pgr_edge_t *edges = NULL;
    size_t total_edges = 0;

    pgr_get_edges(edges_sql, &edges, &total_edges);
    PGR_DBG("Total %ld edges in query:", total_edges);

    PGR_DBG("Starting processing");
    clock_t start_t = clock();
    char *log_msg = NULL;
    char *notice_msg = NULL;
    char *err_msg = NULL;
    do_pgr_kruskal(
            edges, total_edges,
            rootsArr, size_rootsArr,
            order_by,
            max_depth,
            distance,

            result_tuples,
            result_count,
            &log_msg,
            &notice_msg,
            &err_msg);

    time_msg(" processing pgr_kruskal", start_t, clock());
    PGR_DBG("Returning %ld tuples", *result_count);

    if (err_msg) {
        if (*result_tuples) pfree(*result_tuples);
    }
    pgr_global_report(log_msg, notice_msg, err_msg);

    if (edges) pfree(edges);
    if (log_msg) pfree(log_msg);
    if (notice_msg) pfree(notice_msg);
    if (err_msg) pfree(err_msg);

    pgr_SPI_finish();
}

PGDLLEXPORT Datum kruskal(PG_FUNCTION_ARGS) {
    FuncCallContext     *funcctx;
    TupleDesc           tuple_desc;

    pgr_kruskal_t *result_tuples = NULL;
    size_t result_count = 0;

    if (SRF_IS_FIRSTCALL()) {
        MemoryContext   oldcontext;
        funcctx = SRF_FIRSTCALL_INIT();
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);


        PGR_DBG("Calling process");
        /*
           TEXT,             -- Edge sql
           BIGINT,           -- tree root for traversal
           order_by TEXT,
           max_depth INTEGER
        */
        process(
                text_to_cstring(PG_GETARG_TEXT_P(0)),
                PG_GETARG_ARRAYTYPE_P(1),
                text_to_cstring(PG_GETARG_TEXT_P(2)),
                PG_GETARG_INT64(3),
                PG_GETARG_FLOAT8(4),
                &result_tuples,
                &result_count);

#if PGSQL_VERSION > 95
        funcctx->max_calls = result_count;
#else
        funcctx->max_calls = (uint32_t)result_count;
#endif
        funcctx->user_fctx = result_tuples;
        if (get_call_result_type(fcinfo, NULL, &tuple_desc)
                != TYPEFUNC_COMPOSITE) {
            ereport(ERROR,
                    (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                     errmsg("function returning record called in context "
                         "that cannot accept type record")));
        }

        funcctx->tuple_desc = tuple_desc;
        MemoryContextSwitchTo(oldcontext);
    }

    funcctx = SRF_PERCALL_SETUP();
    tuple_desc = funcctx->tuple_desc;
    result_tuples = (pgr_kruskal_t*) funcctx->user_fctx;

    if (funcctx->call_cntr < funcctx->max_calls) {
        HeapTuple    tuple;
        Datum        result;
        Datum        *values;
        bool*        nulls;

        size_t num  = 7;
        values = palloc(num * sizeof(Datum));
        nulls = palloc(num * sizeof(bool));


        size_t i;
        for (i = 0; i < num; ++i) {
            nulls[i] = false;
        }

        // postgres starts counting from 1
        values[0] = Int64GetDatum(funcctx->call_cntr + 1);
        values[1] = Int64GetDatum(result_tuples[funcctx->call_cntr].depth);
        values[2] = Int64GetDatum(result_tuples[funcctx->call_cntr].from_v);
        values[3] = Int64GetDatum(result_tuples[funcctx->call_cntr].node);
        values[4] = Int64GetDatum(result_tuples[funcctx->call_cntr].edge);
        values[5] = Float8GetDatum(result_tuples[funcctx->call_cntr].cost);
        values[6] = Float8GetDatum(result_tuples[funcctx->call_cntr].agg_cost);
        /**********************************************************************/

        tuple = heap_form_tuple(tuple_desc, values, nulls);
        result = HeapTupleGetDatum(tuple);
        SRF_RETURN_NEXT(funcctx, result);
    } else {

        SRF_RETURN_DONE(funcctx);
    }
}
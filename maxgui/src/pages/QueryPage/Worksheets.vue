<template>
    <div class="fill-height worksheet-wrapper">
        <div class="d-flex flex-row">
            <v-tabs
                v-model="activeWkeID"
                show-arrows
                hide-slider
                :height="wkeNavHeight"
                class="tab-navigation--btn-style wke-navigation flex-grow-0"
                :style="{ maxWidth: `calc(100% - ${pageToolbarBtnWidth + 1}px)` }"
            >
                <v-tab
                    v-for="wke in worksheets_arr"
                    :key="wke.id"
                    :href="`#${wke.id}`"
                    class="pa-0 tab-btn text-none"
                    active-class="tab-btn--active"
                >
                    <v-tooltip
                        :disabled="!Boolean(wke.curr_cnct_resource.name)"
                        top
                        transition="slide-x-transition"
                        content-class="shadow-drop"
                    >
                        <template v-slot:activator="{ on }">
                            <div
                                style="min-width:160px"
                                class="fill-height d-flex align-center justify-space-between px-3"
                                v-on="on"
                            >
                                <div class="d-inline-flex align-center">
                                    <span
                                        class="tab-name d-inline-block text-truncate"
                                        style="max-width:88px"
                                    >
                                        {{ wke.name }}
                                    </span>
                                    <template v-for="(value, name) in query_results_map">
                                        <v-progress-circular
                                            v-if="wke.id === name && value.loading_query_result"
                                            :key="name"
                                            class="ml-2"
                                            size="16"
                                            width="2"
                                            color="primary"
                                            indeterminate
                                        />
                                    </template>
                                </div>
                                <v-btn
                                    v-if="worksheets_arr.length > 1"
                                    class="ml-1 del-wke-btn"
                                    icon
                                    x-small
                                    :disabled="is_querying_map[wke.id]"
                                    @click="handleDeleteWke(worksheets_arr.indexOf(wke))"
                                >
                                    <v-icon
                                        size="8"
                                        :color="is_querying_map[wke.id] ? '' : 'error'"
                                    >
                                        $vuetify.icons.close
                                    </v-icon>
                                </v-btn>
                            </div>
                        </template>
                        <span class="color text-text py-2 px-4">
                            {{ $t('connectedTo') }}
                            {{ $typy(wke, 'curr_cnct_resource.name').safeString }}
                        </span>
                    </v-tooltip>
                </v-tab>
            </v-tabs>
            <page-toolbar ref="pageToolbar" @get-total-btn-width="pageToolbarBtnWidth = $event" />
        </div>
        <worksheet-toolbar ref="wkeToolbar" />
        <keep-alive>
            <worksheet
                v-if="activeWkeID"
                :key="activeWkeID"
                :ctrDim="ctrDim"
                :style="{
                    height: `calc(100% - ${wkeNavHeight + 45}px)`,
                }"
                @onCtrlEnter="() => $refs.wkeToolbar.handleRun('selected')"
                @onCtrlShiftEnter="() => $refs.wkeToolbar.handleRun('all')"
                @onCtrlS="() => $refs.pageToolbar.openFavoriteDialog()"
            />
        </keep-alive>
    </div>
</template>
<script>
/*
 * Copyright (c) 2020 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2026-07-11
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

import { mapActions, mapMutations, mapState } from 'vuex'
import Worksheet from './Worksheet'
import WorksheetToolbar from './WorksheetToolbar'
import PageToolbar from './PageToolbar.vue'

export default {
    name: 'worksheets',
    components: {
        Worksheet,
        WorksheetToolbar,
        PageToolbar,
    },
    props: {
        ctrDim: { type: Object, required: true },
    },
    data() {
        return {
            wkeNavHeight: 32,
            pageToolbarBtnWidth: 128,
        }
    },
    computed: {
        ...mapState({
            worksheets_arr: state => state.query.worksheets_arr,
            active_wke_id: state => state.query.active_wke_id,
            curr_cnct_resource: state => state.query.curr_cnct_resource,
            query_results_map: state => state.query.query_results_map,
            is_querying_map: state => state.query.is_querying_map,
        }),
        activeWkeID: {
            get() {
                return this.active_wke_id
            },
            set(v) {
                if (v) this.SET_ACTIVE_WKE_ID(v)
            },
        },
    },
    methods: {
        ...mapMutations({
            SET_ACTIVE_WKE_ID: 'query/SET_ACTIVE_WKE_ID',
        }),
        ...mapActions({
            handleDeleteWke: 'query/handleDeleteWke',
        }),
    },
}
</script>
<style lang="scss" scoped>
.wke-navigation {
    border-left: 1px solid $table-border !important;
    border-top: 1px solid $table-border !important;
    .tab-btn {
        &:first-of-type {
            border-left: none !important;
        }
        border-bottom: none !important;
        border-top: none !important;
        .del-wke-btn {
            visibility: hidden;
        }
        &:hover {
            .del-wke-btn {
                visibility: visible;
            }
        }
    }
}
</style>

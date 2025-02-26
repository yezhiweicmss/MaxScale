<template>
    <!-- Main panel contains editor pane and visualize-sidebar pane -->
    <split-pane
        v-model="mainPanePct"
        class="main-pane__content"
        :minPercent="minMainPanePct"
        split="vert"
        disable
    >
        <template slot="pane-left">
            <!-- Editor pane contains editor and result pane -->
            <split-pane
                ref="editorResultPane"
                v-model="editorPct"
                split="horiz"
                :minPercent="minEditorPct"
            >
                <template slot="pane-left">
                    <split-pane
                        v-model="queryPanePct"
                        class="editor__content"
                        :minPercent="minQueryPanePct"
                        split="vert"
                        :disable="isChartMaximized || !showVisChart"
                    >
                        <!-- Editor pane contains editor and chart pane -->
                        <template slot="pane-left">
                            <query-editor
                                ref="queryEditor"
                                v-model="allQueryTxt"
                                class="editor pt-2 pl-2"
                                :cmplList="getDbCmplList"
                                isKeptAlive
                                @on-selection="SET_SELECTED_QUERY_TXT($event)"
                                v-on="$listeners"
                            />
                        </template>
                        <template slot="pane-right">
                            <chart-container
                                v-if="!$typy(chartOpt, 'data.datasets').isEmptyArray"
                                v-model="chartOpt"
                                :containerHeight="chartContainerHeight"
                                class="chart-pane"
                                @close-chart="setDefChartOptState"
                            />
                        </template>
                    </split-pane>
                </template>
                <template slot="pane-right">
                    <query-result
                        ref="queryResultPane"
                        :dynDim="resultPaneDim"
                        class="query-result"
                        @place-to-editor="placeToEditor"
                        @on-dragging="draggingTxt"
                        @on-dragend="dropTxtToEditor"
                    />
                </template>
            </split-pane>
        </template>
        <template slot="pane-right">
            <visualize-sidebar v-model="chartOpt" class="visualize-sidebar" />
        </template>
    </split-pane>
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
import QueryEditor from '@/components/QueryEditor'
import QueryResult from './QueryResult'
import { mapGetters, mapMutations, mapState } from 'vuex'
import VisualizeSideBar from './VisualizeSideBar'
import ChartContainer from './ChartContainer'
export default {
    name: 'txt-editor-container',
    components: {
        'query-editor': QueryEditor,
        QueryResult,
        'visualize-sidebar': VisualizeSideBar,
        ChartContainer,
    },
    props: {
        dim: { type: Object, required: true },
    },
    data() {
        return {
            // split-pane states
            mainPanePct: 100,
            minMainPanePct: 0,
            editorPct: 60,
            minEditorPct: 0,
            queryPanePct: 100,
            minQueryPanePct: 0,
            mouseDropDOM: null, // mouse drop DOM node
            mouseDropWidget: null, // mouse drop widget while dragging to editor
            maxVisSidebarPx: 250,
            // visualize-sidebar and chart-container state
            defChartOpt: {
                type: '',
                data: {},
                scaleLabels: { x: '', y: '' },
                axesType: { x: '', y: '' },
                isMaximized: false,
            },
            chartOpt: null,
        }
    },
    computed: {
        ...mapState({
            show_vis_sidebar: state => state.query.show_vis_sidebar,
            query_txt: state => state.query.query_txt,
            is_sidebar_collapsed: state => state.query.is_sidebar_collapsed,
        }),
        ...mapGetters({
            getDbCmplList: 'query/getDbCmplList',
        }),
        showVisChart() {
            const datasets = this.$typy(this.chartOpt, 'data.datasets').safeArray
            return Boolean(this.$typy(this.chartOpt, 'type').safeString && datasets.length)
        },
        isChartMaximized() {
            return this.$typy(this.chartOpt, 'isMaximized').safeBoolean
        },
        chartContainerHeight() {
            return (this.dim.height * this.editorPct) / 100
        },
        maxVisSidebarPct() {
            return this.$help.pxToPct({
                px: this.maxVisSidebarPx,
                containerPx: this.dim.width,
            })
        },
        allQueryTxt: {
            get() {
                return this.query_txt
            },
            set(value) {
                this.SET_QUERY_TXT(value)
            },
        },
        resultPaneDim() {
            const visSideBarWidth = this.show_vis_sidebar ? this.maxVisSidebarPx : 0
            return {
                width: this.dim.width - visSideBarWidth,
                height: (this.dim.height * (100 - this.editorPct)) / 100,
            }
        },
    },
    watch: {
        isChartMaximized(v) {
            if (v) this.queryPanePct = this.minQueryPanePct
            else this.queryPanePct = 50
        },
        showVisChart(v) {
            if (v) {
                this.queryPanePct = 50
                this.minQueryPanePct = this.$help.pxToPct({
                    px: 32,
                    containerPx: this.dim.width,
                })
            } else this.queryPanePct = 100
        },
        'dim.height'(v) {
            if (v) this.handleSetMinEditorPct()
        },
        'dim.width'() {
            this.handleSetVisSidebar(this.show_vis_sidebar)
        },
        show_vis_sidebar(v) {
            this.handleSetVisSidebar(v)
        },
    },
    created() {
        this.setDefChartOptState()
    },
    methods: {
        ...mapMutations({
            SET_QUERY_TXT: 'query/SET_QUERY_TXT',
            SET_SELECTED_QUERY_TXT: 'query/SET_SELECTED_QUERY_TXT',
        }),
        setDefChartOptState() {
            this.chartOpt = this.$help.lodash.cloneDeep(this.defChartOpt)
        },
        handleSetMinEditorPct() {
            this.minEditorPct = this.$help.pxToPct({ px: 26, containerPx: this.dim.height })
        },
        handleSetVisSidebar(showVisSidebar) {
            if (!showVisSidebar) this.mainPanePct = 100
            else this.mainPanePct = 100 - this.maxVisSidebarPct
        },
        // editor related functions
        placeToEditor(text) {
            this.$refs.queryEditor.insertAtCursor({ text })
        },
        handleGenMouseDropWidget(dropTarget) {
            /**
             *  Setting text cusor to all elements as a fallback method for firefox
             *  as monaco editor will fail to get dropTarget position in firefox
             *  So only add mouseDropWidget when user agent is not firefox
             */
            if (navigator.userAgent.includes('Firefox')) {
                if (dropTarget) document.body.className = 'cursor--all-text'
                else document.body.className = 'cursor--all-grabbing'
            } else {
                const { editor, monaco } = this.$refs.queryEditor
                document.body.className = 'cursor--all-grabbing'
                if (dropTarget) {
                    const preference = monaco.editor.ContentWidgetPositionPreference.EXACT
                    if (!this.mouseDropDOM) {
                        this.mouseDropDOM = document.createElement('div')
                        this.mouseDropDOM.style.pointerEvents = 'none'
                        this.mouseDropDOM.style.borderLeft = '2px solid #424f62'
                        this.mouseDropDOM.innerHTML = '&nbsp;'
                    }
                    this.mouseDropWidget = {
                        mouseDropDOM: null,
                        getId: () => 'drag',
                        getDomNode: () => this.mouseDropDOM,
                        getPosition: () => ({
                            position: dropTarget.position,
                            preference: [preference, preference],
                        }),
                    }
                    //remove the prev cusor widget first then add
                    editor.removeContentWidget(this.mouseDropWidget)
                    editor.addContentWidget(this.mouseDropWidget)
                } else if (this.mouseDropWidget) editor.removeContentWidget(this.mouseDropWidget)
            }
        },
        draggingTxt(e) {
            const { editor } = this.$refs.queryEditor
            // build mouseDropWidget
            const dropTarget = editor.getTargetAtClientPoint(e.clientX, e.clientY)
            this.handleGenMouseDropWidget(dropTarget)
        },
        dropTxtToEditor(e) {
            if (e.target.textContent) {
                const { editor, monaco, insertAtCursor } = this.$refs.queryEditor
                const dropTarget = editor.getTargetAtClientPoint(e.clientX, e.clientY)

                if (dropTarget) {
                    const dropPos = dropTarget.position
                    // create range
                    const range = new monaco.Range(
                        dropPos.lineNumber,
                        dropPos.column,
                        dropPos.lineNumber,
                        dropPos.column
                    )
                    let text = e.target.textContent.trim()
                    insertAtCursor({ text, range })
                    if (this.mouseDropWidget) editor.removeContentWidget(this.mouseDropWidget)
                }
                document.body.className = ''
            }
        },
    },
}
</script>

<style lang="scss" scoped>
.editor,
.visualize-sidebar,
.query-result,
.chart-pane {
    border: 1px solid $table-border;
    width: 100%;
    height: 100%;
}
.editor {
    border-top: none;
}
</style>

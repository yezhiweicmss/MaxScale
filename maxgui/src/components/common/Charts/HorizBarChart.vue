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
import { HorizontalBar } from 'vue-chartjs'
export default {
    extends: HorizontalBar,
    props: {
        chartData: { type: Object, required: true },
        options: { type: Object },
    },
    watch: {
        chartData() {
            this.$data._chart.destroy()
            this.renderHorizBarChart()
        },
    },
    beforeDestroy() {
        if (this.$data._chart) this.$data._chart.destroy()
    },
    mounted() {
        this.renderHorizBarChart()
    },
    methods: {
        renderHorizBarChart() {
            let chartOption = {
                plugins: {
                    streaming: false,
                },
                scales: {
                    xAxes: [
                        {
                            gridLines: {
                                drawBorder: true,
                            },
                        },
                    ],
                    yAxes: [
                        {
                            gridLines: {
                                drawBorder: false,
                            },
                        },
                    ],
                },
            }
            this.renderChart(this.chartData, this.$help.lodash.deepMerge(chartOption, this.options))
        },
    },
}
</script>

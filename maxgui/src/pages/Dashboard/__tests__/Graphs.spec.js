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

import mount from '@tests/unit/setup'
import Graphs from '@/pages/Dashboard/Graphs'

import { dummy_all_servers } from '@tests/unit/utils'
import store from 'store'

describe('Graphs index', () => {
    let wrapper, axiosStub

    beforeEach(() => {
        axiosStub = sinon.stub(store.$http, 'get').resolves(
            Promise.resolve({
                data: {},
            })
        )
        wrapper = mount({
            shallow: false,
            component: Graphs,

            computed: {
                sessions_datasets: () => [
                    {
                        label: 'Total sessions',
                        type: 'line',
                        backgroundColor: 'rgba(171,199,74,0.1)',
                        borderColor: 'rgba(171,199,74,1)',
                        borderWidth: 1,
                        lineTension: 0,
                        data: [{ x: 1596440973122, y: 30 }],
                    },
                ],
                server_connections_datasets: () => [
                    {
                        label: 'CONNECTIONS',
                        type: 'line',
                        backgroundColor: 'rgba(171,199,74,0.1)',
                        borderColor: 'rgba(171,199,74,1)',
                        borderWidth: 1,
                        lineTension: 0,
                        data: [{ x: 1596440973122, y: 10 }],
                    },
                ],
                threads_datasets: () => [
                    {
                        label: 'LOAD',
                        type: 'line',
                        backgroundColor: 'rgba(171,199,74,0.1)',
                        borderColor: 'rgba(171,199,74,1)',
                        borderWidth: 1,
                        lineTension: 0,
                        data: [{ x: 1596440973122, y: 20 }],
                    },
                ],

                all_servers: () => dummy_all_servers,
            },
        })
    })

    afterEach(() => {
        axiosStub.restore()
    })

    it(`Should update graphs by first sending requests in parallel to
      get all servers, monitors, sessions, services and maxscale threads`, async () => {
        // this prevent fetch loop in line-chart-stream
        await wrapper.setData({
            chartOptionsWithOutCallBack: null,
            mainChartOptions: null,
        })
        //mockup update chart
        await wrapper.vm.updateChart()

        await axiosStub.getCall(0).should.have.been.calledWith('/servers')
        await axiosStub.getCall(1).should.have.been.calledWith('/monitors')
        await axiosStub.getCall(2).should.have.been.calledWith('/sessions')
        await axiosStub.getCall(3).should.have.been.calledWith('/services')
        await axiosStub.lastCall.should.have.been.calledWith(
            '/maxscale/threads?fields[threads]=stats'
        )
    })
})

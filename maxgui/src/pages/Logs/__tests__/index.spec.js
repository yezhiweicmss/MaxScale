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
import Logs from '@/pages/Logs'

describe('Logs index', () => {
    let wrapper
    beforeEach(() => {
        wrapper = mount({
            shallow: false,
            component: Logs,
        })
    })

    it(`Should not show log-container when logViewHeight is not calculated yet`, () => {
        expect(wrapper.vm.$data.logViewHeight).to.be.equal(0)
        const logContainer = wrapper.findComponent({ name: 'log-container' })
        expect(logContainer.exists()).to.be.false
    })
})

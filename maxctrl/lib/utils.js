/*
 * Copyright (c) 2016 MariaDB Corporation Ab
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

var colors = require("colors/safe");

module.exports.strip_colors = function (input) {
  // Based on the regex found in: https://github.com/jonschlinkert/strip-color
  return input.replace(/\x1B\[[(?);]{0,2}(;?\d)*./g, "");
};

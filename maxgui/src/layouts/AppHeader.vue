<template>
    <v-app-bar
        height="50px"
        class="header pl-12 pr-2"
        fixed
        clipped-left
        app
        flat
        color="deep-ocean"
    >
        <v-toolbar-title class="app-headline text-h5">
            <router-link to="/dashboard/servers">
                <img src="@/assets/logo.svg" alt="MariaDB Logo" />
                <span class="product-name tk-azo-sans-web font-weight-medium  white--text">
                    {{ $t('productName') }}
                </span>
            </router-link>
        </v-toolbar-title>

        <v-spacer></v-spacer>

        <v-menu
            v-model="isProfileOpened"
            allow-overflow
            transition="slide-y-transition"
            offset-y
            content-class="mariadb-select-v-menu mariadb-select-v-menu--no-border"
        >
            <template v-slot:activator="{ on }">
                <v-btn dark class="mr-0 arrow-toggle" text tile v-on="on">
                    <v-icon class="mr-1 " size="30">
                        $vuetify.icons.user
                    </v-icon>
                    <span class="user-name tk-adrianna text-capitalize font-weight-regular">
                        {{ logged_in_user ? logged_in_user.name : '' }}
                    </span>

                    <v-icon
                        :class="[isProfileOpened ? 'arrow-up' : 'arrow-down']"
                        size="14"
                        class="mr-0 ml-1 "
                        left
                    >
                        $vuetify.icons.arrowDown
                    </v-icon>
                </v-btn>
            </template>

            <v-list dark class="color bg-color-navigation">
                <v-list-item @click="handleLogout">
                    <v-list-item-title>{{ $t('logout') }}</v-list-item-title>
                </v-list-item>
            </v-list>
        </v-menu>
    </v-app-bar>
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
import { mapActions, mapState } from 'vuex'
export default {
    name: 'app-header',

    data() {
        return {
            items: [],
            isProfileOpened: false,
        }
    },
    computed: {
        ...mapState('user', {
            logged_in_user: state => state.logged_in_user,
        }),
    },
    methods: {
        ...mapActions('user', ['logout']),
        handleLogout() {
            this.logout()
        },
    },
}
</script>
<style lang="scss" scoped>
.header {
    background: linear-gradient(to right, #013545 0%, #064251 100%);
    ::v-deep .v-toolbar__content {
        padding: 4px 10px;
    }
}

.app-headline {
    a {
        text-decoration: none;
    }

    img {
        vertical-align: middle;
        width: 155px;
        height: 38px;
    }

    .product-name {
        position: relative;
        vertical-align: middle;
        font-size: 1.125rem;
    }
}
.user-name {
    font-size: 1rem;
}
.v-btn {
    letter-spacing: normal;
}
</style>

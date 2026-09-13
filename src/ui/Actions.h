#pragma once

// что можно нажать. списки идут диапазонами: id = база + номер строки
enum Action {
    ActNone = -1,
    ActMinimize = 1,
    ActCloseWindow,
    ActConnect,
    ActNavBase,          // +0..3 вкладки
    ActPaste = 20,
    ActPingAll,
    ActRefreshAll,
    ActAddField,
    ActAddSubmit,
    ActAutoSelect,
    ActServerCard,
    ActReconnect,
    ActModeBase = 40,    // +0..2 режимы приложений
    ActBypassBanks,
    ActDirectRu,
    ActDomainField,
    ActDomainAdd,
    ActAppSearch,
    ActDnsBase = 60,     // +0..2
    ActIpv6,
    ActAutoConnect,
    ActAutostart,
    ActLogRefresh,
    ActLogCopy,
    ActLogFolder,
    ActGithub,

    ActServerBase = 1000,
    ActServerDelete = 2000,
    ActSubRefresh = 3000,
    ActSubDelete = 4000,
    ActAppBase = 5000,
    ActDomainBase = 6000,
};

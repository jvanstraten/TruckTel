export type AppInfo = {
    appDirectory: string,
    title: string | null,
    subtitle: string | null,
    text: string | null,
    link: string | null,
    disableLauncher: boolean,
    port: number,
    errorMessage: string | null,
};

export type PluginInfo = {
    pluginVersion: string,
    gameVersion: string,
    localIpAddress: string | null,
    mdnsHostname: string | null,
    apps: AppInfo[],
};

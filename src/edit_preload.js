const { contextBridge, ipcRenderer } = require("electron");

contextBridge.exposeInMainWorld("api", {
    close: () => ipcRenderer.send("close-app"),
    forward_false: () => ipcRenderer.send("forward_false"),
    forward_true: () => ipcRenderer.send("forward_true"),
    get_window_props: () => ipcRenderer.invoke("get_window_props")
});
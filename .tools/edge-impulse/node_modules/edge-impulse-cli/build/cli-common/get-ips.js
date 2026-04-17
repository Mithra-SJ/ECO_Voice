'use strict';
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.ips = void 0;
const os_1 = __importDefault(require("os"));
const ifaces = os_1.default.networkInterfaces();
let ret = [];
Object.keys(ifaces).forEach(ifname => {
    let alias = 0;
    let f = ifaces[ifname];
    if (!f)
        return;
    f.forEach(iface => {
        if ('IPv4' !== iface.family || iface.internal !== false) {
            return;
        }
        if (alias >= 1) {
            // this single interface has multiple ipv4 addresses
            ret.push({ ifname: ifname + ':' + alias, address: iface.address, mac: iface.mac });
        }
        else {
            // this interface has only one ipv4 adress
            ret.push({ ifname: ifname, address: iface.address, mac: iface.mac });
        }
        alias++;
    });
});
exports.ips = ret;
//# sourceMappingURL=get-ips.js.map
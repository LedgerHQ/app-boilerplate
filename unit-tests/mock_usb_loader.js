const Module = require('module')

const originalLoad = Module._load

Module._load = function (request, parent, isMain) {
  if (request === 'usb') {
    const usbStub = {
      Device: function () {},
      getDeviceList: () => [],
    }
    usbStub.Device.prototype = {}
    usbStub.Device.prototype.constructor = usbStub.Device
    usbStub.on = () => {}
    usbStub.removeListener = () => {}
    return usbStub
  }
  return originalLoad.apply(this, arguments)
}

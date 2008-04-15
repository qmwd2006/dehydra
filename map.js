function Map() {
}

Map.prototype.put = function (key, value) {
  this[key.hashcode()] = value
}

Map.prototype.get = function (key) {
  return this[key.hashcode()]
}

Map.prototype.has = function (key) {
  return this.hasOwnProperty(key.hashcode())
}

Map.prototype.remove = function (key) {
  delete this[key.hashcode()]
}

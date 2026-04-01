class FakeDevice:
    def __init__(self):
        self.battery = 82
        self.key = "F13"

    def set_key(self, key):
        print("绑定按键:", key)
        self.key = key

    def get_status(self):
        return {"battery": self.battery, "key": self.key}
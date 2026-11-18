import csv

with open('decoder--261111-161938.csv', newline='') as f:
    reader = csv.reader(f)
    state = 'wait-write'
    accel_val = []
    for row in reader:
        ev = row[2]
        if state == 'wait-write' and ev == 'Address write: 32':
            state = 'wait-reg'
        elif state == 'wait-reg' and ev[:12] == 'Data write: ':
            if len(accel_val) > 0 or ev[12:] == 'A8':
                state = 'wait-read'
            else:
                state = 'wait-write'
        elif state == 'wait-read' and ev == 'Address read: 33':
            state = 'wait-lsb'
        elif state == 'wait-lsb' and ev[:11] == 'Data read: ':
            state = 'wait-msb'
            val = ev[11:]
        elif state == 'wait-msb' and ev[:11] == 'Data read: ':
            state = 'wait-write'
            val = int(ev[11:] + val, 16)
            if val & 0x8000:
                val -= 0x10000
            accel_val.append(val / 16)
            if len(accel_val) == 3:
                print("{},{},{}".format(*accel_val))
                accel_val.clear()



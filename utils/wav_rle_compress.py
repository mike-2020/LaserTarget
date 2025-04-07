
import rle, sys

def rle_encode(data):
    encoding = bytearray()
    prev_char = ''
    count = 1

    if not data: return encoding

    for char in data:
        # If the prev and current characters
        # don't match...
        if char != prev_char:
            # ...then add the count and character
            # to our encoding
            if prev_char:
                #encoding += str(count) + prev_char
                encoding.append(count)
                encoding.append(prev_char)
            count = 1
            prev_char = char
        else:
            # Or increment our counter
            # if the characters do match
            count += 1
    else:
        # Finish off the encoding
        #encoding += str(count) + prev_char
        encoding.append(count)
        encoding.append(prev_char)
        return encoding

src_name = sys.argv[1]
with open(src_name, "rb") as file:
    content = file.read()


values,counts = rle.encode(content[44:])
print(counts)
encoding = bytearray()
for i in range(0, len(values)):
    n = counts[i]
    while n > 0:
        if n > 127:
            encoding.append(127)
            n = n -127
        else:
            encoding.append(n)
            n = 0
        encoding.append(values[i])

with open(src_name+".rle", "wb+") as file:
    file.write(content[0:44])
    file.write(encoding)
FROM alpine AS build-stage

ARG URL
RUN [ -n "$URL" ] || { echo "ERROR: Set build arg! --build-arg URL=\$(git remote get-url origin)"; exit 1; }

RUN apk update && apk add build-base cmake git python3 libusb-dev gcc pkgconf bash

RUN git clone $URL

WORKDIR /ezp2019
RUN ./build.sh

FROM alpine AS release
RUN apk update && apk add libusb --no-cache
COPY --from=build-stage /ezp2019 /ezp2019
RUN cp /ezp2019/build/ezp2019 /usr/local/bin/ezp2019
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
CMD ["sh"]

FROM alpine AS build-stage

#ARG URL
#RUN [ -n "$URL" ] || { echo "ERROR: Set build arg! --build-arg URL=\$(git remote get-url origin)"; exit 1; }

RUN apk update && apk add build-base cmake python3 libusb-dev gcc pkgconf bash

COPY . /ezp2019

WORKDIR /ezp2019
RUN ./build.sh

FROM alpine AS release
RUN apk update && apk add libusb --no-cache
COPY --from=build-stage /ezp2019/build/ezp2019 /usr/local/bin/ezp2019
COPY --from=build-stage /ezp2019/build/libezp2019.so.0.0.0 /usr/local/lib/libezp2019.so.0.0.0
RUN ln -s /usr/local/lib/libezp2019.so.0.0.0 /usr/local/lib/libezp2019.so
#RUN cp /ezp2019/build/ezp2019 /usr/local/bin/ezp2019
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
CMD ["sh"]

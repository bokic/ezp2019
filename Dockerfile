FROM alpine AS build-stage

ARG VERSION
ENV VERSION=${VERSION}

RUN apk add --no-cache build-base cmake python3 libusb-dev gcc pkgconf bash
COPY . /ezp2019
WORKDIR /ezp2019
RUN ./build.sh && cmake --install build --prefix /install

FROM alpine AS release
RUN apk add --no-cache libusb
COPY --from=build-stage /install/ /usr/local/
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
CMD ["sh"]

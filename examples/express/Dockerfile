FROM node:22-alpine
WORKDIR /app
RUN echo '{"name":"plumbr-express-example","version":"1.0.0","dependencies":{"express":"^5.1.0"}}' > package.json
RUN npm install
COPY server.js .
EXPOSE 3000
CMD ["node", "server.js"]

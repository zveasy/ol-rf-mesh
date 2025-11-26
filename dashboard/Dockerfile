FROM node:20-alpine AS build
WORKDIR /app
COPY package.json tsconfig.json vite.config.ts ./
COPY public ./public
COPY src ./src
RUN npm install && npm run build

FROM nginx:alpine
COPY --from=build /app/dist /usr/share/nginx/html
EXPOSE 80

// Minimal typing for the vendored qrcode-generator (MIT, Kazuhiko Arase).
export interface QRCode {
  addData(data: string): void;
  make(): void;
  getModuleCount(): number;
  isDark(row: number, col: number): boolean;
}
declare const qrcode: (typeNumber: number, errorCorrectionLevel: 'L' | 'M' | 'Q' | 'H') => QRCode;
export default qrcode;

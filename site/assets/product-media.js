// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
// Everything shown on the Where to Buy page comes from this one file.
//
// To swap a concept render for a real photograph:
//   1. Put the photo in site/media/product/ (any size; 4:3 landscape at 1600 x 1200 or larger works best).
//   2. Change that image's `src` below, and set `kind: 'photo'`. The "Concept render" label disappears.
// Images are cropped to the frame, never stretched, so a different size or ratio won't break the layout.
//
// To confirm an item in the box, set `confirmed: true`. To add one, add a line.
// When pricing or purchasing is ready, fill in `availability` (add `links` to show buttons).
window.POCKETOS_PRODUCT = {
  images: {
    hero:      { src: 'media/product/device-front.jpg', kind: 'render', alt: 'PocketOS Device standing upright, showing the companion on its round display' },
    angle:     { src: 'media/product/device-angle.jpg', kind: 'render', alt: 'PocketOS Device lying flat, showing the aluminium case, side buttons and menu screen' },
    boxClosed: { src: 'media/product/box-closed.jpg',   kind: 'render', alt: 'Closed PocketOS box with the logo in silver foil, the device standing beside it' },
    boxOpen:   { src: 'media/product/box-open.jpg',     kind: 'render', alt: 'Open PocketOS box with the device in its cradle and the lid standing behind' },
    boxInsert: { src: 'media/product/box-insert.jpg',   kind: 'render', alt: 'The box insert with its round device cradle, finger notches and accessory compartment' },
    contents:  { src: 'media/product/in-the-box.jpg',   kind: 'render', alt: 'The PocketOS Device, a coiled USB-C cable and a quick start card laid out side by side' },
    size:      { src: 'media/product/size.jpg',         kind: 'render', alt: 'PocketOS from the front and from the side, about 55 mm across and about 15 mm deep' },
    scale:     { src: 'media/product/scale.jpg',        kind: 'render', alt: 'PocketOS lying beside a smartphone, for scale' },
    home:      { src: 'media/product/home.jpg',         kind: 'render', alt: 'PocketOS standing on a wooden bedside table beside a mug and a book' }
  },

  // "Take a closer look" on the Where to Buy page. A slot without an image shows a tidy placeholder
  // until a real photograph is added (set `image` to one of the keys above).
  closer: [
    { image: 'hero',    title: 'Front',            text: 'The round AMOLED screen and its companion.' },
    { image: 'angle',   title: 'Side and buttons',  text: 'The aluminium case, with the Power and Boot buttons on the side.' },
    { image: 'size',    title: 'Size',             text: 'About 55 mm across and 15 mm deep.' },
    { image: 'scale',   title: 'Next to your phone', text: 'Smaller than a phone, easy to carry from room to room.' },
    { image: 'home',    title: 'Around the home',  text: 'On a bedside table, sofa arm or desk.' },
    { image: 'boxOpen', title: 'Unboxing',         text: 'Face up in its fitted cradle, ready to switch on.' }
  ],

  packaging: [
    { image: 'boxClosed', title: 'The box', text: 'Matte black, with the PocketOS logo in silver foil on the lid.' },
    { image: 'boxOpen',   title: 'Opened',  text: 'The device sits face up in a fitted cradle, ready to switch on.' },
    { image: 'boxInsert', title: 'Inside',  text: 'A moulded insert with a cradle for the device, finger notches to lift it out and a separate compartment for accessories.' }
  ],

  inTheBox: {
    image: 'contents',
    items: [
      { name: 'PocketOS Device', detail: 'With PocketOS installed and ready to set up.', confirmed: true },
      { name: 'USB-C charging cable', detail: 'For charging PocketOS from a USB-C charger or computer.', confirmed: true },
      { name: 'Quick start guide', detail: 'A printed guide to switching on and setting up.', confirmed: true }
    ]
  },

  availability: {
    price: '£59.99',
    standardPrice: '£99.99',
    status: 'Coming soon',
    region: 'UK only',
    note: 'PocketOS isn’t on sale yet. Ordering details will appear here when it is. To begin with, PocketOS is sold and delivered in the United Kingdom only.',
    links: [] // e.g. { label: 'Buy now', href: 'https://…' }
  }
};

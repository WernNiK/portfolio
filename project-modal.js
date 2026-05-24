const projectCards = document.querySelectorAll('.project-card');
const modal = document.getElementById('projectModal');
const closeBtn = document.getElementById('projectModalClose');
const titleEl = document.getElementById('projectModalTitle');
const descEl = document.getElementById('projectModalDescription');
const toolsEl = document.getElementById('projectModalTools');
const ctaEl = document.getElementById('projectModalCta');
const messageCtaEl = document.getElementById('projectModalMessageCta');
const carouselImageEl = document.getElementById('projectCarouselImage');
const carouselEl = document.getElementById('projectCarousel');
const carouselDotsEl = document.getElementById('projectCarouselDots');
const carouselPrevEl = document.getElementById('projectCarouselPrev');
const carouselNextEl = document.getElementById('projectCarouselNext');
const lightboxEl = document.getElementById('projectImageLightbox');
const lightboxImageEl = document.getElementById('projectLightboxImage');

let currentImages = [];
let currentIndex = 0;

const setLinkState = (el, href) => {
    if (href) {
        el.href = href;
        el.classList.remove('is-disabled');
        if (/\.(apk|zip)($|\?)/i.test(href)) {
            el.setAttribute('download', '');
        } else {
            el.removeAttribute('download');
        }
    } else {
        el.removeAttribute('href');
        el.removeAttribute('download');
        el.classList.add('is-disabled');
    }
};

const renderCarousel = () => {
    if (!carouselImageEl || !carouselDotsEl || !currentImages.length) return;

    carouselImageEl.src = currentImages[currentIndex];
    carouselDotsEl.innerHTML = '';

    currentImages.forEach((_, idx) => {
        const dot = document.createElement('button');
        dot.className = `project-carousel-dot${idx === currentIndex ? ' is-active' : ''}`;
        dot.setAttribute('aria-label', `Go to image ${idx + 1}`);
        dot.addEventListener('click', () => {
            currentIndex = idx;
            renderCarousel();
        });
        carouselDotsEl.appendChild(dot);
    });
};

const openModal = (card) => {
    const modalTitle = card.dataset.modalTitle || 'Project';
    const modalDescription = card.dataset.modalDescription || '';
    const modalTools = (card.dataset.modalTools || '')
        .split('|')
        .map((tool) => tool.trim())
        .filter(Boolean);
    const modalImages = (card.dataset.modalImages || '')
        .split(',')
        .map((src) => src.trim())
        .filter(Boolean);
    const apk = (card.dataset.apk || '').trim();
    const website = (card.dataset.website || '').trim();

    titleEl.textContent = modalTitle;
    descEl.textContent = modalDescription;

    toolsEl.innerHTML = '';
    modalTools.forEach((tool) => {
        const li = document.createElement('li');
        li.textContent = tool;
        toolsEl.appendChild(li);
    });

    currentImages = modalImages.length ? modalImages : [card.style.backgroundImage.replace(/^url\(["']?/, '').replace(/["']?\)$/, '')];
    currentIndex = 0;
    renderCarousel();

    const primary = website || apk;
    setLinkState(ctaEl, primary);

    modal.classList.add('is-open');
    modal.setAttribute('aria-hidden', 'false');
    document.body.style.overflow = 'hidden';
};

const openLightbox = () => {
    if (!lightboxEl || !lightboxImageEl || !currentImages.length) return;
    lightboxImageEl.src = currentImages[currentIndex] || carouselImageEl?.src || '';
    if (!lightboxImageEl.src) return;
    lightboxEl.classList.add('is-open');
    lightboxEl.setAttribute('aria-hidden', 'false');
};

const closeLightbox = () => {
    if (!lightboxEl || !lightboxImageEl) return;
    lightboxEl.classList.remove('is-open');
    lightboxEl.setAttribute('aria-hidden', 'true');
    lightboxImageEl.src = '';
};

const closeModal = () => {
    closeLightbox();
    modal.classList.remove('is-open');
    modal.setAttribute('aria-hidden', 'true');
    document.body.style.overflow = '';
};

projectCards.forEach((card) => {
    card.addEventListener('click', () => openModal(card));
    card.addEventListener('keydown', (event) => {
        if (event.key === 'Enter' || event.key === ' ') {
            event.preventDefault();
            openModal(card);
        }
    });
});

if (closeBtn) closeBtn.addEventListener('click', closeModal);

if (messageCtaEl) {
    messageCtaEl.addEventListener('click', (event) => {
        event.preventDefault();
        closeModal();
        const contactSection = document.getElementById('contact');
        if (contactSection) {
            contactSection.scrollIntoView({ behavior: 'smooth', block: 'start' });
        }
    });
}

if (carouselPrevEl) {
    carouselPrevEl.addEventListener('click', () => {
        if (!currentImages.length) return;
        currentIndex = (currentIndex - 1 + currentImages.length) % currentImages.length;
        renderCarousel();
    });
}

if (carouselNextEl) {
    carouselNextEl.addEventListener('click', () => {
        if (!currentImages.length) return;
        currentIndex = (currentIndex + 1) % currentImages.length;
        renderCarousel();
    });
}

if (carouselImageEl) {
    carouselImageEl.style.cursor = 'zoom-in';
    carouselImageEl.setAttribute('role', 'button');
    carouselImageEl.setAttribute('tabindex', '0');
    const handleImageActivate = (event) => {
        event.preventDefault();
        event.stopPropagation();
        openLightbox();
    };
    carouselImageEl.addEventListener('click', handleImageActivate);
    carouselImageEl.addEventListener('keydown', (event) => {
        if (event.key === 'Enter' || event.key === ' ') {
            handleImageActivate(event);
        }
    });
}

if (carouselEl) {
    carouselEl.addEventListener('click', (event) => {
        const isNav = event.target.closest('.project-carousel-nav');
        const isDot = event.target.closest('.project-carousel-dot');
        const isImage = event.target.closest('#projectCarouselImage');
        if (isImage) return;
        if (isNav || isDot) return;
        openLightbox();
    });
}

if (lightboxEl) {
    lightboxEl.addEventListener('click', (event) => {
        if (event.target === lightboxEl) {
            closeLightbox();
        }
    });
}

if (modal) {
    modal.addEventListener('click', (event) => {
        if (event.target === modal) {
            closeModal();
        }
    });
}

document.addEventListener('keydown', (event) => {
    if (event.key === 'Escape') {
        if (lightboxEl?.classList.contains('is-open')) {
            closeLightbox();
            return;
        }
        if (modal?.classList.contains('is-open')) {
            closeModal();
        }
    }
});
